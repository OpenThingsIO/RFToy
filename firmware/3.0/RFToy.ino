/* RFToy (ESP8266-based) Firmware
 * Tong Shen and Rui Wang
 * Dec 2017 @ openthings.io
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <Arduino.h>
#include <EEPROM.h> // Flash memory library
#include <SSD1306.h> // LCD library
#include <RCSwitch.h> // Radio library
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.h>

#include "LibrationFont.h" // Font used on LCD display
#include "htmls.h"

#define DEBUG
#ifdef DEBUG
  #define PRINT(x)   Serial.print(x)
  #define PRINTLN(x) Serial.println(x)
#else
  #define PRINT(x)   {}
  #define PRINTLN(x) {}
#endif

// ====== PIN DEFINES ======
#define MAIN_I2CADDR 0x20           // PCF8574 IO Expander I2C address
#define MAIN_INPUTMASK 0b00001010
#define IOEXP_PIN 0x80              // base for any pin on the IO expander
#define RECEIVE_POWER_PIN  IOEXP_PIN+0
#define TRANSMIT_POWER_PIN IOEXP_PIN+2
#define B1 IOEXP_PIN+1  // buttons 1,2,3
#define B2 0
#define B3 IOEXP_PIN+3
#define TRANSMIT_PIN 16
#define RECEIVE_PIN  14
#define LED_PIN 2

// For simplicity, assume 24-bit code, protocol 1
// The RCSwitch library supports more general code 
#define CODE_LENGTH 24
#define PROTOCOL 1

// ====== UI States ======
#define UI_MENU 0
#define UI_CODE 1
#define UI_RECORD_ON    2
#define UI_RECORD_OFF   3
#define UI_TRANSMIT_ON  4
#define UI_TRANSMIT_OFF 5
#define UI_DELETE   6
#define UI_DISP_IP  7
#define UI_RESET_WIFI 8
#define UI_RESET_ALL  9

#define STATION_COUNT_PER_PAGE 5
#define STATION_COUNT 50

#define PROMPT_TIMEOUT 15  // 15 seconds countdown
#define POWER_PAUSE 500

// ====== EEPROM Defines ======
#define STATION_NAME_SIZE 20
#define EEPROM_SIZE 2048
#define SIG_ADDR EEPROM_SIZE-8  // address where signature string is stored

// ====== UI variables ======
int station_selected = 0;
int mode = UI_MENU;
int old_mode = UI_CODE;
boolean redraw = false;
int usewifi = 1;
boolean hasPsk = false;

// ====== Station data ======
struct StationStruct {
  uint32_t on;
  uint32_t off;
  uint16_t delay;
  char name[STATION_NAME_SIZE];
  boolean status;
};

StationStruct *stations;  // station data: memory mapped to EEPROM

// ====== Button status ======
struct ButtonStruct {
  byte pin;
  byte status;
  boolean pressed;
  boolean long_press;
  unsigned long t;
};

ButtonStruct buttons[] = {
  {B1, HIGH, false, false, 0},
  {B2, HIGH, false, false, 0},
  {B3, HIGH, false, false, 0}
};

#define BUTTON_COUNT (sizeof(buttons)/sizeof(ButtonStruct))

// Raw signal sampling
enum class RawSamplingMode{TRANSMIT, RECEIVE};
volatile unsigned long next;
volatile byte* sampleBuf = NULL;
volatile unsigned long sampleCounter=0;

// Note: ESP8266 runs at 80 MHz
// So one clock cycle is 1/80 microsecond
unsigned long sampleInterval = 10; // in microsecond
unsigned long sampleTime = 1; // in second
unsigned long sampleSize = 0;
boolean isrOn = false;

// ====== Library objects ======
// ====== OLED, RCSwitch, Server ======
SSD1306 display(0x3c, 4, 5);
RCSwitch mySwitch = RCSwitch();
ESP8266WebServer server(80);
unsigned long resetTimeout = 0;

void prepareReset(int s) { resetTimeout = millis()+s; }

// ====== Forward declares of functions ======
String getStationName(int);
void setMode(int);
void deleteCurrentStation();
void radioRecord(int, boolean);
void radioTransmit(int, boolean);
String get_ip();
String get_mac();
void set_led(int);
int get_led();
char dec2hexchar(byte);

// ====== PCF8574 IO Expander functions ======

byte pcf_read(int addr) { // read a byte
  Wire.beginTransmission(addr);
  Wire.requestFrom(addr, 1);
  byte data = Wire.read();
  Wire.endTransmission();
  return data;
}

void pcf_write(int addr, byte data) { // write a byte
  Wire.beginTransmission(addr);
  Wire.write(data);
  Wire.endTransmission();
}

// Extension of Arduino's basic pinMode function 
// to handle standard + IO expander pins
void pinModeExt(byte pin, byte mode) {
  if(pin>=IOEXP_PIN) {
    // PCF8574 does not require explicit pin mode
    // nothing to do
  } else {
    pinMode(pin, mode);
  }
}

// Extension of Arduino's basic digitalWrite function 
// to handle standard + IO expander pins
void digitalWriteExt(byte pin, byte value) {
  if(pin>=IOEXP_PIN) {
    // a pin on IO expander
    byte data=pcf_read(MAIN_I2CADDR);
    if(value) data|=(1<<(pin-IOEXP_PIN));
    else     data&=~(1<<(pin-IOEXP_PIN));
    data |= MAIN_INPUTMASK; // make sure to enforce 1 for input pins
    pcf_write(MAIN_I2CADDR, data);
  } else {
    digitalWrite(pin, value);
  }
}

// Extension of Arduino's basic digitalRead function 
// to handle standard + IO expander pins
byte digitalReadExt(byte pin) {
  if(pin>=IOEXP_PIN) {
    // a pin on IO expander
    return pcf_read(MAIN_I2CADDR)&(1<<(pin-IOEXP_PIN));
  } else {
    return digitalRead(pin);
  }
}

// ====== EEPROM functions ======

void eeprom_init(bool force=false) {
  EEPROM.begin(EEPROM_SIZE);
  stations = (StationStruct*)EEPROM.getDataPtr();

  char* eptr = (char*)EEPROM.getDataPtr();
  // check signature
  if(!force && eptr[SIG_ADDR]=='E' && eptr[SIG_ADDR+1]=='S' && eptr[SIG_ADDR+2]=='P') {
    // signature set, read configurations
    PRINTLN(F("EEPROM initialized"));
  } else {
    // signature not set, clear and initialize EEPROM
    PRINTLN(F("Reset EEPROM"));
    memset((void*)EEPROM.getDataPtr(), 0, EEPROM_SIZE);
    // initialize station names
    for(byte i=0;i<STATION_COUNT;i++) {
      String name = "Station ";
      name += ((i+1)/10);
      name += ((i+1)%10);
      strcpy(stations[i].name, name.c_str());
    }
    eptr[SIG_ADDR] = 'E';
    eptr[SIG_ADDR+1] = 'S';
    eptr[SIG_ADDR+2] = 'P';
    EEPROM.commit();
  }  
}

// ====== UI functions ======

void setupButtons(){
  ButtonStruct *pb = buttons;
  for(int i = 0; i < BUTTON_COUNT; i++, pb++){
    pinModeExt(pb->pin,INPUT_PULLUP);
  }
}

void reset_buttons(){
  ButtonStruct *pb = buttons;
  for(int i = 0; i < BUTTON_COUNT; i++, pb++){
    if(pb->pressed) { pb->status = HIGH; }
    pb->pressed = false;
    pb->long_press = false;
  }
}

void detect_buttons(){
  ButtonStruct *pb = buttons;  
  for(int i = 0; i < BUTTON_COUNT; i++, pb++){
    if(digitalReadExt(pb->pin) == LOW){
      if(pb->status == HIGH){
        pb->t = millis();
        pb->status = LOW;
      } else {
        if(millis() - pb->t > 1000){
          // turn on LED to indicate long press
          set_led(HIGH);
        }
      }
    } else {
      if(pb->status == LOW){
        pb->status = HIGH;
        pb->pressed = true;
        if(millis() - pb->t > 1000){
          pb->long_press = true;
        }
        pb->t = 0;
        set_led(LOW);
      }
    }
  }
  delay(50);  // simple delay debounce
}

void process_buttons(){
  detect_buttons();
  switch(mode){
  case UI_MENU:
    if(buttons[0].pressed){
      if(buttons[0].long_press){
        setMode(UI_RESET_ALL);
      } else {
        station_selected = (station_selected-1+STATION_COUNT)%STATION_COUNT;
        setMode(UI_MENU);
      }
    }
    if(buttons[1].pressed){
      if(buttons[1].long_press){
        setMode(UI_RESET_WIFI);
      } else {
        setMode(UI_CODE);
      }
    }
    if(buttons[2].pressed){
      if(buttons[2].long_press){
        setMode(UI_DISP_IP);
      } else {
        station_selected = (station_selected+1)%STATION_COUNT;
        setMode(UI_MENU);
      }
    }
    break;
  case UI_CODE:
    if(buttons[0].pressed){
      if(buttons[0].long_press){
        setMode(UI_RECORD_ON);
      } else {
        setMode(UI_TRANSMIT_ON);
      }
    }
    if(buttons[1].pressed){
      if(buttons[1].long_press){
        setMode(UI_DELETE);
      } else {
        setMode(UI_MENU);
      }
    }
    if(buttons[2].pressed){
      if(buttons[2].long_press){
        setMode(UI_RECORD_OFF);
      } else {
        setMode(UI_TRANSMIT_OFF);
      }
    }
    break;
  case UI_DELETE:
    if(buttons[2].pressed){
      setMode(old_mode);
    }
    if(buttons[0].pressed){
      deleteCurrentStation();
      setMode(old_mode);
    }
    break;
  case UI_DISP_IP:
    if(buttons[2].pressed) {
      setMode(old_mode);
    }
    break;
  case UI_RESET_WIFI:
  case UI_RESET_ALL:
    if(buttons[2].pressed) {
      setMode(old_mode);
    }
    if(buttons[0].pressed) {
      // reset WiFi
      WiFi.disconnect(true);
      if(mode==UI_RESET_ALL) {
        eeprom_init(true);
      }
      prepareReset(1000);
      setMode(old_mode);
    }
    break;
  }
  reset_buttons();
}

// ====== LCD functions ======

String getHex(unsigned long num,int chars){
  String s = "";
  for(int i = chars-1; i >= 0; i--){
    if(num == 0){
      s += "-";
    } else {
      s += String(((num >> i*4) & 0xF),HEX);
    }
  }
  return s;
}

String getStationCode(int sid){
  String s = "";
  s += getHex(stations[sid].on,6);
  s += getHex(stations[sid].off,6);
  s += getHex(stations[sid].delay,4);
  s.toUpperCase();
  return s;
}

// Get center of string given the number of characters
// This is for displaying string onto LCD
uint8_t getStrCenterOfs(uint8_t size){
  if (size < 0 || size > 21)  return 0;
  uint8_t fontWidth = 6;
  return 64 - (6*size)/2;
}

// Set display mode
void setMode(int m){
  display.clear();
  old_mode = mode;
  mode = m;
  redraw = true;
}

void uiCountDown(int timer, boolean (*callback)()=NULL) {
  unsigned long t = millis(), to = t+timer*1000;
  String td = "";
  while(1) {
    if(millis() > to) break;
    if(callback()) break;
    display.setColor(BLACK);
    display.drawString(100, 52, td);
    display.setColor(WHITE);
    td = (PROMPT_TIMEOUT-(millis()-t)/1000);
    display.drawString(100, 52, td);
    display.display();
    delay(100);
  }
}

boolean selectWiFi() {
  if (!digitalReadExt(B1)) { // select WiFi
    usewifi = 1;
    delay(250);
    return true;
  }
  if (!hasPsk && !digitalReadExt(B2)) { // select WiFi
    usewifi = 2;
    delay(250);
    return true;
  }
  if (!digitalReadExt(B3)) {  // select manual
    usewifi = 0;
    delay(250);
    return true;
  }
  return false;
}

boolean dummyInstructions() {
  if (!digitalReadExt(B1) || !digitalReadExt(B2) || !digitalReadExt(B3)) {
    delay(250);
    return true;
  }
  return false;
}

// Prompt screen, appear at the beginning
void promptScreen() {
  display.drawString(getStrCenterOfs(5), 2, F("RFToy"));
  display.drawString(0, 12, F("https://openthings.io"));
  display.drawString(4, 23, F("Enable WiFi?"));
  String ssid = WiFi.psk().c_str();
  hasPsk = ssid != NULL && ssid.length() > 0;
  if (hasPsk){
    display.drawString(10, 35, F("B1: Yes"));
    display.drawString(10, 45, F("B3: No"));
  } else {
    display.drawString(4, 35, F("B1:Manual, B2:by WPS"));
    display.drawString(4, 45, F("B3:No"));    
  }
  display.display();

  uiCountDown(PROMPT_TIMEOUT, selectWiFi);
  display.clear();  
  
  // Bypass second splash screen if user hit B2 earlier
  if (usewifi < 2){ 
    display.drawString(0, 2,  F("    Instructions    "));
    display.drawString(0, 13, F(" click / hold button"));
    display.drawString(0, 23, F("B1: <- / reset all"));
    display.drawString(0, 33, F("B2: OK / reset WiFi"));
    display.drawString(0, 43, F("B3: -> / show IP"));
    display.display();

    uiCountDown(PROMPT_TIMEOUT, dummyInstructions);
    display.clear();
  }
}

void wpsFailed() {
  display.clear(); 
  display.drawString(getStrCenterOfs(10), 2, F("WPS failed!"));
  display.drawString(4, 35, F("Reset the device to"));
  display.drawString(4, 45, F("try again"));
  display.display();
  delay(3000); 
  ESP.reset();
  delay(1000);
}

void uiDrawMenu(){
  uint8_t w, h, ofs;
  display.drawString(getStrCenterOfs(12), 2, F("Station List"));
  h = 10;
  w = 6;
  ofs = 15; // 0-15 pixel is yellow title pixel
  // the first station number on this page
  int stationNum = ( station_selected / STATION_COUNT_PER_PAGE ) * STATION_COUNT_PER_PAGE;
  for(int i = 0; i < STATION_COUNT_PER_PAGE; i++){
    uint8_t lh = i*h+ofs; // the height of this line in pixel
    if(stationNum == station_selected){
      display.drawString(0,lh,">");
    }
    display.drawString(w,lh,String(stationNum+1));
    display.drawString(w*3,lh,":");
    if(stations[stationNum].status) {
      display.fillRect(w*4,lh,128-w*4,h);
      display.setColor(BLACK);
    }
    if(stations[stationNum].on==0 && stations[stationNum].off==0){
      display.drawString(w*4,lh, "-");
    } else {
      display.drawString(w*4,lh,stations[stationNum].name);
    }
    display.setColor(WHITE);
    stationNum++;
  }
  display.display();
}

void uiDrawStation(){
  String title = stations[station_selected].name;
  if(stations[station_selected].status) {
    display.fillRect(0, 2, 128, 12);
    display.setColor(BLACK);
  }
  display.drawString(getStrCenterOfs(title.length()), 2, title);
  display.setColor(WHITE);
  display.drawString(0,15, F("Click B1/B3: play"));
  display.drawString(0,27, F("Hold  B1/B3: record"));
  display.drawString(0,52, F("on    back/del    off"));
  if(stations[station_selected].on==0 && stations[station_selected].off==0){
    display.drawString(0,39, "Code:-");
  } else {
    display.drawString(0,39,"Code:"+getStationCode(station_selected));
  }
  display.display();
}

void uiDrawIP() {
  display.drawString(0, 2, F("Device IP:"));
  display.drawString(0, 14, get_ip());
  display.drawString(0, 26, F("MAC address:"));
  display.drawString(0, 38, get_mac());
  display.drawString(0, 50, F("Click B3 to go back."));
  display.display();
}

void uiDrawReset(boolean all=false) {
  display.drawString(0, 15, all?F("Reset All?"):F("Reset WiFi?"));
  display.drawString(0,50, F("B1:yes         B3:no"));
  display.display();
}

void record(boolean on){
  display.drawString(28, 20, F("Recording"));
  display.drawString(28, 32, on?F("On code"):F("Off code"));
  display.display();

  radioRecord(station_selected, on);
  setMode(UI_CODE);
}

void transmit(boolean on){
  display.drawString(28, 20, F("Transmitting"));
  display.drawString(28, 32, on?F("On code"):F("Off code"));
  display.display();

  radioTransmit(station_selected, on);
  setMode(old_mode);
}

void deleteCurrentStation(){
  memset(stations+station_selected, 0, sizeof(StationStruct));
  String name = "Station ";
  name += ((station_selected+1)/10);
  name += ((station_selected+1)%10);
  strcpy(stations[station_selected].name, name.c_str());
  stations = (StationStruct*)EEPROM.getDataPtr(); // forces dirty bit to be set to 1
  EEPROM.commit();
}

void uiDeleteStation(){
  display.drawString(0, 15, F("Confirm deletion of"));
  display.drawString(0, 25, F("code?"));
  display.drawString(0, 50, F("yes               no"));
  if(stations[station_selected].on==0 && stations[station_selected].off==0){
    display.drawString(0,2,"-");
  } else {
    display.drawString(0,2,stations[station_selected].name);
  }
  display.display();
}

// ====== Radio functions ======

void radioRecord(int staNum, boolean on){
  digitalWriteExt(RECEIVE_POWER_PIN,HIGH);
  delay(POWER_PAUSE);
  pinModeExt(RECEIVE_PIN, INPUT);
  mySwitch.enableReceive(RECEIVE_PIN);

  unsigned long t = millis();
  while(millis() - t < 10000){
    yield();
    if(mySwitch.available()){
      if(mySwitch.getReceivedValue() != 0){
        if(on){
          stations[staNum].on = mySwitch.getReceivedValue();
          stations[staNum].delay = mySwitch.getReceivedDelay();
        } else {
          stations[staNum].off = mySwitch.getReceivedValue();
          stations[staNum].delay = mySwitch.getReceivedDelay();
        }
        // forces dirty bit to be set to 1
        stations = (StationStruct*)EEPROM.getDataPtr();
        EEPROM.commit();
      }
      mySwitch.resetAvailable();
      break;
    }
  }
  mySwitch.disableReceive();
  digitalWriteExt(RECEIVE_POWER_PIN,LOW);
}

void radioTransmit(int staNum, boolean on){
  if(stations[staNum].delay != 0){
    digitalWriteExt(TRANSMIT_POWER_PIN,HIGH);
    delay(POWER_PAUSE);
    mySwitch.enableTransmit(TRANSMIT_PIN);
    mySwitch.setProtocol(PROTOCOL);
    mySwitch.setPulseLength(stations[staNum].delay*.98);
    if(on){
      if(stations[staNum].on != 0){
        mySwitch.send(stations[staNum].on,CODE_LENGTH);
      }
    } else {
      if(stations[staNum].off != 0){
        mySwitch.send(stations[staNum].off,CODE_LENGTH);
      }
    }
    mySwitch.disableTransmit();
    digitalWriteExt(TRANSMIT_POWER_PIN,LOW);
    stations = (StationStruct*)EEPROM.getDataPtr();
    stations[staNum].status = on;
    EEPROM.commit();
  }
}

// Transmit Interrupt Service Routine
// ESP8266 requires ISR to have ICACHE_RAM_ATTR qualifier
ICACHE_RAM_ATTR void transmitSampleISR(){
  if(sampleCounter >= (sampleSize * 8)){
    timer0_detachInterrupt();
    isrOn = false;
    sampleCounter=0;
    digitalWriteExt(TRANSMIT_POWER_PIN, LOW);
    return;    
  }

  // set next timestamp this ISR is called
  next=ESP.getCycleCount()+80*sampleInterval;
  timer0_write(next);

  int bytePos = sampleCounter / 8;
  int bitPos = sampleCounter % 8;
  byte result = sampleBuf[bytePos] & (1<<(7-bitPos));
  // transmit one bit
  digitalWrite(TRANSMIT_PIN, result); 
  sampleCounter++;
}

// ESP8266 requires ISR to have ICACHE_RAM_ATTR qualifier
ICACHE_RAM_ATTR void receiveSampleISR(){
  if(sampleCounter >= (sampleSize * 8)){
    timer0_detachInterrupt();
    isrOn = false;
    sampleCounter=0;
    digitalWriteExt(RECEIVE_POWER_PIN, LOW);
    return;    
  }

  // set next timestamp this ISR is called
  next=ESP.getCycleCount()+80*sampleInterval;
  timer0_write(next);

  int bytePos = sampleCounter / 8;
  int bitPos = sampleCounter % 8;
  // receive one bit
  if(digitalRead(RECEIVE_PIN)){
    sampleBuf[bytePos] |= (1<<(7-bitPos));
  } else {
    sampleBuf[bytePos] &= ~(1<<(7-bitPos));
  }
  sampleCounter++;
}

// Setup Interrupt Service Routines
void startInterrupt(RawSamplingMode opMode){
  noInterrupts();
  isrOn = true;
  sampleCounter=0;
  timer0_isr_init();
  switch(opMode){
    case RawSamplingMode::RECEIVE:
      pinModeExt(RECEIVE_PIN, INPUT);
      digitalWriteExt(RECEIVE_POWER_PIN, HIGH);
      delay(POWER_PAUSE);
      timer0_attachInterrupt(receiveSampleISR);
    break;
    case RawSamplingMode::TRANSMIT:
      pinModeExt(TRANSMIT_PIN, OUTPUT);
      digitalWriteExt(TRANSMIT_POWER_PIN, HIGH);
      delay(POWER_PAUSE);
      timer0_attachInterrupt(transmitSampleISR);
    break;
    default:
    break;  
  }
  next=ESP.getCycleCount()+80*sampleInterval;
  timer0_write(next);
  interrupts();
}

// ====== Server functions ======
#define HTTP_FAIL    -1
#define HTTP_SUCCESS 0

void server_send_result(int result) {
  String str = F("{\"result\":");
  str += result;
  str += "}";
  server.send(200, "application/json", str);
}

void server_send_json(String json) {
  server.send(200, "application/json", json);
}

void server_send_html(String html) {
  server.send(200, "text/html", html);
}

void changeController(){
  int sid = server.arg("sid").toInt();
  if(sid<0||sid>=STATION_COUNT) {
    server_send_result(HTTP_FAIL);
    return;
  }
  boolean success = true;
  if (server.hasArg("name")) {
    // change name
    String name = server.arg("name");
    stations = (StationStruct*)EEPROM.getDataPtr();
    int len = name.length()+1;
    if(len>=STATION_NAME_SIZE) len=STATION_NAME_SIZE;
    strncpy(stations[sid].name, name.c_str(), len);
    stations[sid].name[STATION_NAME_SIZE-1]=0;
    EEPROM.commit();
  } else if (server.hasArg("record")) {
    if (server.arg("record") == "on") {	
      // record on signal
      radioRecord(sid, true);
    } else if (server.arg("record") == "off"){
      // record off signal
      radioRecord(sid, false);
    } else { success = false; }
  } else if (server.hasArg("turn")) {
    if (server.arg("turn") == "on") {
      // send on signal
      radioTransmit(sid, true);
      server_send_result(HTTP_SUCCESS);
    } else if (server.arg("turn") == "off") {
      // send off signal
      radioTransmit(sid, false);
      server_send_result(HTTP_SUCCESS);
    } else { success = false; }
  }
  server_send_result(success?HTTP_SUCCESS:HTTP_FAIL);
  setMode(mode);
}

void getController(){
  String msg = "{\"stations\":[";
  for (int i = 0; i < STATION_COUNT; ++i) {
    msg += "{";
    msg += F("\"name\":\"");
    msg += stations[i].name;
    msg += F("\",\"status\":");
    msg += stations[i].status ? 1 : 0;
    msg += F(",\"code\":\"");
    msg += getHex(stations[i].on, 6);
    msg += getHex(stations[i].off, 6);
    msg += getHex(stations[i].delay,4);
    msg += "\"}";
    if (i < (STATION_COUNT-1))  msg += ",";
  }
  msg += "]}";
  server_send_json(msg);
}

/* Actions send to server:
  scan --> Starting monitoring the radio signal, should return the length of the monitor period
  fetch --> Asking the server for data to draw the plot, usually after the monitor action
  transmit --> Asking the server to transmit the signal
*/

void handleRawSampling(){
  if (server.hasArg("action"))
  {
    if (server.arg("action") == "scan")
    {
      String msg = "{";
      msg += F("\"time\":");
      msg += sampleTime;
      msg += F(",\"interval\":");
      msg += sampleInterval;
      msg += "}";
      server_send_json(msg);
      server.handleClient();

      startInterrupt(RawSamplingMode::RECEIVE);
      //delay(sampleTime*1000);
      return;

    }else if(server.arg("action") == "fetch"){
      server.sendContent("HTTP/1.1 200 OK\r\nContent-Type:application/json\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\n\r\n");
      String msg = "{\"data\":\"";
      for (int i = 0; i < sampleSize; i++)
      {
        byte record = sampleBuf[i];
        msg += dec2hexchar((record>>4)&0x0f);
        msg += dec2hexchar(record&0x0f);
        if(msg.length()>1200) {
          server.sendContent(msg);
          msg = "";
        }
      }
      msg += "\"}";
      server.sendContent(msg);
      server.client().stop();      
      return;
    }else if(server.arg("action") == "transmit"){
      server_send_result(HTTP_SUCCESS);
      server.handleClient();

      startInterrupt(RawSamplingMode::TRANSMIT);
      //delay(sampleTime*1000);
      return;
    }
    server_send_result(HTTP_FAIL);
  }
}

String get_ip() {
  String ip = "";
  IPAddress _ip = WiFi.localIP();
  ip = _ip[0];
  ip += ".";
  ip += _ip[1];
  ip += ".";
  ip += _ip[2];
  ip += ".";
  ip += _ip[3];
  return ip;
}

char dec2hexchar(byte dec) {
  if(dec<10) return '0'+dec;
  else return 'A'+(dec-10);
}

String get_mac() {
  static String hex = "";
  if(!hex.length()) {
    byte mac[6];
    WiFi.macAddress(mac);

    for(byte i=0;i<6;i++) {
      hex += dec2hexchar((mac[i]>>4)&0x0F);
      hex += dec2hexchar(mac[i]&0x0F);
      if(i!=5) hex += ":";
    }
  }
  return hex;
}

void onUploadFinish() {
  // finish update and check error
  if(!Update.end(true) || Update.hasError()) {
    server_send_result(HTTP_FAIL);
    return;
  }
  server_send_result(HTTP_SUCCESS);
  prepareReset(1000);
}

void onUpload() {
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    WiFiUDP::stopAll();
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace()-0x1000)&0xFFFFF000;
    if(!Update.begin(maxSketchSpace)) {
      PRINTLN(F("not enough space"));
    }
  } else if(upload.status == UPLOAD_FILE_WRITE) {
    PRINT(".");
    if(Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      PRINTLN(F("size mismatch"));
    }

  } else if(upload.status == UPLOAD_FILE_END) {
    
    PRINTLN(F("upload completed"));
       
  } else if(upload.status == UPLOAD_FILE_ABORTED){
    Update.end();
    PRINTLN(F("upload aborted"));
  }
  delay(0);
}
// --------------->>>>>>>>>>>>>> main function <<<<<<<<<<<<<<---------------
#include <Ticker.h>
Ticker ticker;

// LED_PIN on ESP8266 uses reverse logic
void set_led(int status=HIGH) {
  digitalWriteExt(LED_PIN, status?LOW:HIGH);
}

// LED_PIN on ESP8266 uses reverse logic
int get_led() {
  return !digitalReadExt(LED_PIN);
}

void tick()
{
  set_led(!get_led());
}

// gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  //entered config mode, make led toggle faster
  ticker.attach(0.1, tick);
  display.clear();
  display.drawString(getStrCenterOfs(14), 2, F("Configure WiFi"));
  display.drawString(getStrCenterOfs(7), 14, F("AP mode"));
  display.drawString(0, 26, "SSID: "+myWiFiManager->getConfigPortalSSID());
  display.drawString(0, 38, F("Connect to the SSID"));
  display.drawString(0, 50, F("Then goto 192.168.4.1"));
  display.display();
}

// gets called when WiFiManager enters WPS mode
void wpsModeInitScreen () {
  //entered config mode, make led toggle faster
  ticker.attach(0.1, tick);
  display.clear();
  display.drawString(getStrCenterOfs(14), 2, F("Configure WiFi"));
  display.drawString(getStrCenterOfs(12), 14, F("by using WPS"));
  display.drawString(0, 38, F("Hit WPS button"));
  display.drawString(0, 50, F("on your router!"));
  display.display();
}

void saveConfigCallback () {
  display.clear();
  display.drawString(getStrCenterOfs(14), 2, F("Configure WiFi"));
  display.drawString(0, 26, F("Connected!"));
  display.drawString(0, 38, "IP: "+WiFi.localIP().toString());
  display.display();
  delay(2000);
  display.clear();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  setupButtons();
  pinModeExt(TRANSMIT_POWER_PIN,OUTPUT);
  pinModeExt(RECEIVE_POWER_PIN,OUTPUT);
  digitalWriteExt(TRANSMIT_POWER_PIN,LOW);
  digitalWriteExt(RECEIVE_POWER_PIN,LOW);
  pinModeExt(LED_PIN, OUTPUT);
  set_led(LOW);
  
  display.init();
  display.flipScreenVertically();
  display.setFont(Liberation_Mono_10);

  eeprom_init();
  promptScreen();

  redraw = true;

  if(usewifi > 0) {
    PRINTLN(F("Start WiFi"));
    //set led pin as output
    // start ticker with 0.5 because we start in AP mode and try to connect
    ticker.attach(1.0, tick);  	
    WiFiManager wifiManager;
    if (usewifi == 2) {
      wpsModeInitScreen();
      WiFi.mode(WIFI_STA);
      delay(1000);
      bool wpsSuccess = WiFi.beginWPSConfig();
      if (wpsSuccess){
        String newSSID = WiFi.SSID();
        if(newSSID.length() <= 0) {
          PRINTLN(F("failed to connect and hit timeout"));
          //reset and try again
          wpsFailed();
        } else {
          PRINTLN(F("Connected by WPS!"));
          saveConfigCallback();
        }
      } else {
        wpsFailed();
      }
      //wifiManager.setAPCallback(wpsModeCallback);
    } else {
      wifiManager.setAPCallback(configModeCallback);
      wifiManager.setSaveConfigCallback(saveConfigCallback);
      //wifiManager.autoConnect("AutoConnectAP");
      if(!wifiManager.autoConnect()) {
        PRINTLN(F("failed to connect and hit timeout"));
        //reset and try again, or maybe put it to deep sleep
        ESP.reset();
        delay(1000);
      } 
    }
  	
    ticker.detach();
    set_led(LOW);

    server.on("/", [](){ server_send_html(FPSTR(home_html)); });
    server.on("/rs", [](){ server_send_html(FPSTR(rawsample_html)); });
    server.on("/jc", getController);
    server.on("/cc", changeController);
    server.on("/hrs", handleRawSampling);
    // OTA update 
    server.on("/update", HTTP_GET, [](){ server_send_html(FPSTR(update_html)); });
    server.on("/update", HTTP_POST, onUploadFinish, onUpload);    

    PRINTLN(F("Start Server"));
    server.begin();
  }
  // Radio sampling init
  sampleSize = sampleTime * 1000000 / sampleInterval / 8;
  if(!sampleBuf)  sampleBuf = new byte[sampleSize+1];  
}

void loop(){
  if(isrOn) return;
  server.handleClient();
  switch(mode){
  case UI_MENU:
    if(redraw){
      uiDrawMenu();
      redraw = false;
    }
  break;
  case UI_CODE:
    if(redraw){
      uiDrawStation();
      redraw = false;
    }
  break;
  case UI_RECORD_ON:
    record(true);
  break;
  case UI_RECORD_OFF:
    record(false);
  break;
  case UI_TRANSMIT_ON:
    transmit(true);
  break;
  case UI_TRANSMIT_OFF:
    transmit(false);
  break;
  case UI_DELETE:
    if(redraw){
      uiDeleteStation();
      redraw = false;
    }
  break;
  case UI_DISP_IP:
    if(redraw) {
      uiDrawIP();
      redraw = false;
    }
  break;
  case UI_RESET_WIFI:
    if(redraw) {
      uiDrawReset();
      redraw = false;
    }
  break;
  case UI_RESET_ALL:
    if(redraw) {
      uiDrawReset(true);
      redraw = false;
    }
  break;
  }
  process_buttons();
  if(resetTimeout>0 && millis()>resetTimeout) {
    ESP.reset();
    delay(1000);
  }
}
