const char home_html[] PROGMEM = R"(<html>
<head>
<title>RFToy</title>
<meta name='viewport' content='width=device-width,initial-scale=1'>	
<link href="https://fonts.googleapis.com/css?family=Geo|Open Sans" rel="stylesheet">
</head>
<body>
<div style="max-width: 100%; margin: auto;">
<h1 id="title">RFToy</h1>
<div id="menu">
<div id="statusContainer">
<div id="statusIndicator"></div>
<div id="status">Connecting</div>
</div>
<div class="menuDropDownContainer">
<div id="menuIcon">
<div class="menuBar"></div><div class="menuBar"></div><div class="menuBar"></div>
</div>
<div id="menuDropDownContent" class="menuDropDownContentClass">
<div id="menuDropDownRS">Raw Sample</div>
<div id="menuDropDownUp">Firmware</div>
<!--<div>Setting</div>-->
</div>
</div>
</div>

<div class="scrollable">
<script type="text/javascript">
function w(s){ document.write(s); }
for (let i = 1; i < 51; i++) {
w('<div style="margin-top:8px;">');
w('<div style="display:flex;">');
w('<button type="button" class="accordion underScoreBtn">');
w("Station "+i);
w('</button>');
w('<button class="playBtn underScoreBtn">Off</button>');
w('</div>');
w('<div class="panel">');
w('<div class="panelInfo code">');
w('Code: ');
w('</div>');
w('<button class="recordOnBtn underScoreBtn">Record On</button>');
w('<button class="changeNameBtn underScoreBtn">Set Name</button>');
w('<button class="recordOffBtn underScoreBtn">Record Off</button>');
w('</div>');
w('</div>');
}
</script>
</div>
</div>
<style type="text/css">
body{background-color: #eeeeee;}
#title{text-align: center;font-family: 'Geo';}
#status, .panelInfo, .underScoreBtn, .menuDropDownContentClass div{
font-family: 'Open Sans';
font-size: 1.0rem;
}
#menu{
margin: 1.0rem 1.0rem;
}
#statusContainer{
width: calc(100% - 1.5rem);
display: inline-block;
}
#status{
display: inline-block;
margin-left: 0.2rem;
}
#statusIndicator{
width: 0.5rem;
height: 0.5rem;
display: inline-block;
border-radius: 50%;
background-color: #f2f2f2;
padding: none;
border: 1px solid white;
border-top: 1px solid #8c8c8c;
animation: spin 2s linear infinite;
}
@keyframes spin {
0% { transform: rotate(0deg); }
100% { transform: rotate(360deg); }
}
.menuDropDownContainer{
display: inline-block;
position: relative;
}
#menuIcon{
width: 1rem;
display: inline-block;
}
.menuBar{
margin: 0.15rem 0;
width: 1.5rem;
height: 0.15rem;
background-color: black;
}
.menuDropDownContentClass{
display: none;
position: absolute;
background-color: #fff;
min-width: 10rem;
box-shadow: 0rem 0.5rem 1rem 0rem rgba(0,0,0,0.2);
right: 0;
}
.menuDropDownContentClass div{
display: block;
color: black;
padding: 1rem 1.5rem;
text-align: center;
}
.menuDropDownShow{
display: block;
}
.panelInfo{
padding: 8px;
}
.underScoreBtn{
border-style: solid;
border-width: 0px 0px 1px 0px;
border-color: #aaaaaa;
outline: none;
background-color: white;
}
.underScoreBtn:hover:enabled, .accordion.active:hover, .menuDropDownContentClass div:hover{
background-color: #ddd;
}
.scrollable{
overflow-y: scroll;
height: 500px;
}
.playBtn{
width: 23%;
padding: 10px;
margin: 0% 1%;
}
.accordion{
width: 73%;
padding: 10px;
margin: 0% 1%;
text-align: center;
}
button.accordion:before{
content: '\25B6'; /*The triangle at the front*/
color: #8c8c8c;
float: left;
}
button.accordion.active:before{
content: '\25BC';
float: left;
}
button.accordion.active{
background-color: #e6e6e6;
}
div.panel{
max-height: 0;
margin: 0% 3%;
overflow: hidden;
transition: max-height 0.2s ease-out, margin-bottom 0.2s ease-out;
padding: 0%;
}
.recordOnBtn, .recordOffBtn{
width: 31%;
margin: 0% 1%;
padding: 10px;
cursor: pointer;
}
.changeNameBtn{
width: 32%;
margin: 0% 1%;
padding: 10px;
cursor: pointer;
}
</style>
<script type="text/javascript">
var ip="/";
function fail() {alert("Unable to reach controller.");}
function id(s) {return document.getElementById(s);}
function clas(s) {return document.getElementsByClassName(s);}
var MAX_TIMEOUT = 15000;
Connection = { Connected:"Connected", Disconnected:"Disconnected", Timeout:"Timeout", Connecting:"Connecting"}
var cur_connection = Connection.Connecting;
var errorUpdateRate = 30000; // updating the page content once every 30 sec
var connectedUpdateRate = 3000; // updating the page content once every sec
var updateInterval; // store the update interval id

function setButtonState(b,t) {
b.innerHTML = t;
if(t==="On") {b.style.backgroundColor = '#aaffaa'; }
if(t==="Off") { b.style.backgroundColor = '#ffffff'; }
}
function getUpdate(){
if (cur_connection != Connection.Connected) {
changeStatus(Connection.Connecting);
}
let xhr = new XMLHttpRequest();
let url = ip+"jc";
xhr.open("GET", url, true);
xhr.timeout = MAX_TIMEOUT;
xhr.ontimeout = function(){
if (cur_connection == Connection.Connecting || cur_connection == Connection.connected) {
changeStatus(Connection.Timeout);
}
};
xhr.onreadystatechange = function(){
if (xhr.readyState === 4 && xhr.status === 200) {
changeStatus(Connection.Connected);
let acc = clas("accordion");
let pb = clas("playBtn");
let cd = clas("code");
let json = JSON.parse(xhr.responseText);
for (let i = 0; i < json.stations.length; i++) {
acc[i].innerHTML = json.stations[i].name;
if (json.stations[i].status==0) {
setButtonState(pb[i], "Off");
}else{
setButtonState(pb[i], "On");
}
cd[i].innerHTML = "Code: "+json.stations[i].code;
}
}else if(xhr.readyState === 4){
changeStatus(Connection.Disconnected);
}
};
xhr.send();
}
function changeStatus(c){
if (cur_connection != c) {
let s = id("status");
s.innerHTML = c;
let i = id("statusIndicator");
if (cur_connection == Connection.Connecting) {
i.style.border = "none";
i.style.animation = "none";
}
switch (c){
case Connection.Connecting:
i.style.backgroundColor = "#f2f2f2";
i.style.border = "1px solid white";
i.style.borderTop = "1px solid #8c8c8c";
i.style.animation = "spin 2s linear infinite";
break;
case Connection.Connected:
// fast update
if (!updateInterval) {
clearInterval(updateInterval);
updateInterval = setInterval(getUpdate, connectedUpdateRate);
}
i.style.backgroundColor = "#47d147";
break;
case Connection.Timeout:
// clear interval, set the update interval slower
if (updateInterval) {
clearInterval(updateInterval);
updateInterval = setInterval(getUpdate, errorUpdateRate);
}
i.style.backgroundColor = "#ffbb33";
break;
case Connection.Disconnected:
// clear interval, set the update interval slower
if (updateInterval) {
clearInterval(updateInterval);
updateInterval = setInterval(getUpdate, errorUpdateRate);
}
i.style.backgroundColor = "#ff4d4d";
break;
}
}
cur_connection = c;
}

let menuicon = id("menuIcon");
menuicon.onclick = function(){
id("menuDropDownContent").classList.toggle("menuDropDownShow");
}
let rawSampling = id("menuDropDownRS");
rawSampling.onclick = function(){window.open('rs', '_top');}
let firmware = id("menuDropDownUp");
firmware.onclick = function() {window.open('update', '_top');}
let acc = clas("accordion");
for (let i = 0; i < acc.length; i++) {
acc[i].onclick = function(){
// highlight the button that controlls the panel
this.classList.toggle("active");
// get the panel this button control
let panel = this.parentNode.nextElementSibling;
// hide and show the panel
if (panel.style.maxHeight) {
panel.style.maxHeight = null;
panel.style.marginBottom = "0%";
}else{
panel.style.maxHeight = panel.scrollHeight + "px";
panel.style.marginBottom = "2%";
}
}
}
// Change name button
let ccn = clas("changeNameBtn");
for (let i = 0; i < ccn.length; i++) {
ccn[i].onclick = function(){
let btn = ccn[i].parentNode.previousSibling.firstChild;
let input = prompt("Enter the name for "+btn.innerHTML,
"Livingroom Light");
if (input != null) {
if (input.length > 20) {
alert("Name too long (maximum 20 characters).");
}else if(input.length < 1){
alert("Name too short.");
}else {
let xhr = new XMLHttpRequest();
let url = ip+"cc?sid="+i+"&name="+encodeURIComponent(input);
xhr.open("GET", url, true);
//xhr.timeout = MAX_TIMEOUT;
//xhr.ontimeout = timeoutProcedure;
xhr.onreadystatechange = function(){
if (xhr.readyState === 4) {
if (xhr.status === 200) {
btn.innerHTML = input;
}else{
fail();
}
}
};
xhr.send();
}
}
}
// record on signal button
let roBtn = ccn[i].previousSibling;
roBtn.onclick = function(){
this.disabled = true;
this.innerHTML = "Recording..";
let xhr = new XMLHttpRequest();
let url = ip+"cc?sid="+i+"&record=on";
xhr.open("GET", url, true);
//xhr.timeout = MAX_TIMEOUT;
//xhr.ontimeout = timeoutProcedure;
xhr.onreadystatechange = function(){
if (xhr.readyState === 4) {
if (xhr.status != 200) {
fail();
}
}
roBtn.innerHTML = "Record On Signal";
roBtn.disabled = false;
};
xhr.send();
}
// record off signal button
let rfBtn = ccn[i].nextElementSibling;
rfBtn.onclick = function(){
this.disabled = true;
this.innerHTML = "Recording..";
let xhr = new XMLHttpRequest();
let url = ip+"cc?sid="+i+"&record=off";
xhr.open("GET", url, true);
//xhr.timeout = MAX_TIMEOUT;
//xhr.ontimeout = timeoutProcedure;
xhr.onreadystatechange = function(){
if (xhr.readyState === 4) {
if (xhr.status != 200) {
fail();
}
}
rfBtn.innerHTML = "Record Off Signal";
rfBtn.disabled = false;
};
xhr.send();
}
}
// Turn on and off button
let pb = clas("playBtn");
for (let i = 0; i < pb.length; i++) {
pb[i].onclick = function(){
this.disabled = true;
let xhr = new XMLHttpRequest();
let url = ip+"cc?sid="+i+"&turn=";
if (this.innerHTML == "Off") {
this.innerHTML = "Turning On...";
url += "on";
}else{
this.innerHTML = "Turning Off...";
url += "off";
}
xhr.open("GET", url, true);
//xhr.timeout = MAX_TIMEOUT;
//xhr.ontimeout = timeoutProcedure;
xhr.onreadystatechange = function(){
if (xhr.readyState === 4) {
if (xhr.status === 200) {
if (pb[i].innerHTML == "Turning On...") {
setButtonState(pb[i], "On");
}else{
setButtonState(pb[i], "Off");
}
}else{
fail();
if (pb[i].innerHTML == "Turning On...") {
setButtonState(pb[i], "Off");
}else{
setButtonState(pb[i], "On");
}
}	
}
pb[i].disabled = false;
};
xhr.send();
}
}
getUpdate();
updateInterval = setInterval(getUpdate, connectedUpdateRate);
</script>
</body>
</html>
)";
const char rawsample_html[] PROGMEM = R"(<html>
<head>
<meta name='viewport' content='width=device-width,initial-scale=1'>	
<script src="https://cdn.plot.ly/plotly-latest.min.js"></script>
<link href="https://fonts.googleapis.com/css?family=Geo|Open Sans" rel="stylesheet">
</head>
<body>
<div id="tester" style="margin: 2%, 0%;"></div>
<div id="radioBtnContainer">
<button id="backBtn" onclick="window.history.back();">Back</button>
<button id="recBtn">Record</button>
<button id="tranBtn">Transmit</button>
<button id="dlBtn">Data</button>
</div>
<style type="text/css">
body{
background-color: #eeeeee;
margin: none;
}
#radioBtnContainer{
margin: 0% 0%;
}
button{
font-family: 'Open Sans';
font-size: 1.0rem;
border-style: solid;
border-width: 0px 0px 1px 0px;
border-color: #aaaaaa;
outline: none;
background-color: white;
cursor: pointer;
}
button:hover:enabled{
background-color: #ddd;
}
#recBtn, #tranBtn{
width: 22%;
margin: 0% 1%;
padding: 0.8rem;
}
#backBtn, #dlBtn {
width: 16%;
margin: 0% 1%;
padding: 0.8rem;
}			
</style>
<script type="text/javascript">
var ip = "/";
function id(s) {return document.getElementById(s);}
function clas(s) {return document.getElementsByClassName(s);}

var sampleInt=0, sampleTime=0;
var sampleResult=[], sampleTimeline=[];

var transmitBtn = id("tranBtn");
var receiveBtn =  id("recBtn");
var downloadBtn = id("dlBtn");
function plotGraph(){
var trace = {
x: sampleTimeline,
y: sampleResult,
mode: 'lines',
line: {
color: 'rgb(55, 128, 191)',
width: 1
}
};
var dataPoints = [ trace ];
var layout = {
title:'Radio Signal (Signal Length: '+sampleTime+' second)',
xaxis: {
title: 'Time ('+sampleInt+'microsecond)'
},
yaxis: {
title: 'Signal'
},
paper_bgcolor: 'rgba(0,0,0,0)',
plot_bgcolor: 'rgba(0,0,0,0)',
font: { family: "Raleway" }
};
Plotly.newPlot('tester', dataPoints, layout);
}
transmitBtn.onclick = function(){
this.disabled = true;
this.innerHTML = "Transmiting..";
let xhr = new XMLHttpRequest();
let url = ip+"hrs?action=transmit";
xhr.open("GET", url, true);
xhr.onreadystatechange = function(){
if (xhr.readyState === 4 && xhr.status === 200) {
transmitBtn.disabled = false;
transmitBtn.innerHTML = "Transmit";
}
};
xhr.send();
};
receiveBtn.onclick = function(){
this.disabled = true;
let xhr = new XMLHttpRequest();
let url = ip+"hrs?action=scan";
xhr.open("GET", url, true);
//xhr.timeout = MAX_TIMEOUT;
//xhr.ontimeout = timeoutProcedure;
xhr.onreadystatechange = function(){
if (xhr.readyState === 4 && xhr.status === 200) {
receiveBtn.innerHTML = "Scanning..";
// should be getting the time to get data
let json = JSON.parse(xhr.responseText);
sampleTime = json.time;
sampleInt = json.interval;
setTimeout(fetchData, 1000 * sampleTime * 1.2);
}
};
xhr.send();
};
function fetchData(){
receiveBtn.innerHTML = "Fetching Data";
let xhr = new XMLHttpRequest();
let url = ip+"hrs?action=fetch";
xhr.open("GET", url, true);
//xhr.timeout = MAX_TIMEOUT;
//xhr.ontimeout = timeoutProcedure;
xhr.onreadystatechange = function(){
if (xhr.readyState === 4 && xhr.status === 200) {
receiveBtn.innerHTML = "Processing..";
let json = JSON.parse(xhr.responseText);

let fileData = '{"time":'+sampleTime+',"interval":'+sampleInt+',"data":"';
fileData += json.data.toUpperCase();
fileData += '"}';
var textFile = window.URL.createObjectURL(new Blob([fileData], {type: 'application/json'}));
downloadBtn.onclick = function(){ window.open(textFile);}
downloadBtn.disabled = false;
let data = json.data;
let result = [];
while(data.length != 0){
result.push(parseInt(data.substring(0, 2), 16));
data = data.substring(2);
}
var timeline = [];
var resultfull = [];
var j=0;
for (var i = 0; i < result.length; i++) {
for(var k=0; k<8; k+=1, j++) {
timeline[j] = j;
resultfull[j] = (result[i]>>k)&0b1;
}
}
sampleResult = resultfull;
sampleTimeline = timeline;
plotGraph();
receiveBtn.innerHTML = "Record";
receiveBtn.disabled = false;

}
};
xhr.send();
}
// replot the graph on window resized
window.addEventListener("resize", plotGraph);
// draw the empty graph when first loaded the page
plotGraph([], [], "");
// because there isn't any data to be downloaded
downloadBtn.disabled = true;
</script>
</body>
)";
const char update_html[] PROGMEM = R"(<head>
<title>RFToy</title>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<link rel="stylesheet" href="http://code.jquery.com/mobile/1.4.5/jquery.mobile-1.4.5.min.css">
<link href="https://fonts.googleapis.com/css?family=Geo|Open Sans" rel="stylesheet">
<script src="http://code.jquery.com/jquery-1.11.1.min.js"></script>
<script src="http://code.jquery.com/mobile/1.4.5/jquery.mobile-1.4.5.min.js"></script>
</head>
<style type="text/css">
* {font-family: 'Open Sans'; font-size: 1.0rem;}
</style>
<body>
<div data-role='page' id='page_update'>
<div data-role='header'><h3>RFToy Firmware Update</h3></div>
<div data-role='content'>
<form method='POST' action='/update' id='fm' enctype='multipart/form-data'>
<table cellspacing=4>
<tr><td><b>Select firmware file (.bin)</b></td></tr>
<tr><td><input type='file' name='file' accept='.bin' id='file'></td></tr>
<tr><td><label id='msg'></label></td></tr>
</table>
<a href='#' data-role='button' data-inline='true' data-theme='a' id='btn_back'>Back</a>
<a href='#' data-role='button' data-inline='true' data-theme='b' id='btn_submit'>Upload</a>
</form>
</div>
</div>
<script>
function id(s) {return document.getElementById(s);}
function clear_msg() {id('msg').innerHTML='';}
function show_msg(s,t,c) {
id('msg').innerHTML=s.fontcolor(c);
if(t>0) setTimeout(clear_msg, t);
}
function goback() {history.back();}
$('#btn_back').click(function(e){
e.preventDefault(); goback();
});
$('#btn_submit').click(function(e){
var files= id('file').files;
if(files.length==0) {show_msg('Please select a file.',2000,'red'); return;}
var btn = id('btn_submit');
show_msg('Uploading. Please wait...',0,'green');
var fd = new FormData();
var file = files[0];
fd.append('file', file, file.name);
var xhr = new XMLHttpRequest();
xhr.onreadystatechange = function() {
if(xhr.readyState==4 && xhr.status==200) {
var jd=JSON.parse(xhr.responseText);
if(jd.result==0) {
show_msg('Update is successful. Rebooting. Please wait...',10000,'green');
} else {
show_msg('Update failed.',0,'red');
}
}
};
xhr.open('POST', 'update', true);
xhr.send(fd);
});
</script>
</body>
)";
