SKETCH = ./RFToy.ino
LIBS = $(ESP_LIBS)/Wire \
$(ESP_LIBS)/ESP8266WiFi \
$(ESP_LIBS)/DNSServer \
$(ESP_LIBS)/ESP8266WebServer \
$(ESP_LIBS)/EEPROM \
$(ESP_LIBS)/Ticker \
~/Arduino/libraries/SSD1306 \
~/Arduino/libraries/rc-switch \
~/Arduino/libraries/WiFiManager \
.

ESP_ROOT = $(HOME)/esp8266_2.7.4/
ESPCORE_VERSION = 274
BUILD_ROOT = /tmp/$(MAIN_NAME)

UPLOAD_SPEED = 460800
UPLOAD_VERB = -v

FLASH_DEF = 4M3M
FLASH_MODE = dio
FLASH_SPEED = 40 
F_CPU = 80000000L

BOARD = generic

include ./makeEspArduino.mk
