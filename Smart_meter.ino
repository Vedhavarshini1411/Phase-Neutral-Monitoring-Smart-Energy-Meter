#include <WiFi.h>
#include <WebServer.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>

const char* ssid = "Kalam_004";
const char* password = "kalam@2025";

WebServer server(80);

const int phasePin = 34;
const int neutralPin = 35;
const int voltagePin = 32;
const int relayPin = 26;
const int buzzerPin = 25;


const float acsSensitivity = 0.066;
const float adcScale = 5.0 / 3.3;
const float voltageScalingFactor = 230.0 / 0.77;
const int numSamples = 500;
const float theftThreshold = 0.35;

float zeroPhase = 0;
float zeroNeutral = 0;
float zeroVoltage = 0;

float rmsPhase = 0;
float rmsNeutral = 0;
float voltageAC = 0;
bool theftDetected = false;

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(relayPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(relayPin, HIGH);
  digitalWrite(buzzerPin, LOW);

  lcd.init();
  lcd.backlight();

  
  long sumPhase = 0, sumNeutral = 0, sumVoltage = 0;
  for (int i = 0; i < 500; i++) {
    sumPhase += analogRead(phasePin);
    sumNeutral += analogRead(neutralPin);
    sumVoltage += analogRead(voltagePin);
    delay(5);
  }
  zeroPhase = sumPhase / 500.0;
  zeroNeutral = sumNeutral / 500.0;
  zeroVoltage = sumVoltage / 500.0;

 
  WiFi.begin(ssid, password);
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.print(".");
  }
  lcd.clear();
  lcd.print("WiFi Connected");

  server.on("/", handleRoot);
  server.on("/data", sendData);
  server.on("/relay/on", []() {
    digitalWrite(relayPin, HIGH);
    server.send(200, "text/plain", "Relay ON");
  });
  server.on("/relay/off", []() {
    digitalWrite(relayPin, LOW);
    delay(4000);
    server.send(200, "text/plain", "Relay OFF");
  });

  server.begin();
}

void loop() {
  server.handleClient();
  measureAC();
  updateLCD();
}

void measureAC() {
  float sumPhaseSq = 0, sumNeutralSq = 0, sumVoltageSq = 0;

  for (int i = 0; i < numSamples; i++) {
    float vPhase = (analogRead(phasePin) * (3.3 / 4095.0) - zeroPhase * (3.3 / 4095.0)) * adcScale;
    float vNeutral = (analogRead(neutralPin) * (3.3 / 4095.0) - zeroNeutral * (3.3 / 4095.0)) * adcScale;
    float vAC = analogRead(voltagePin) * (3.3 / 4095.0) - zeroVoltage * (3.3 / 4095.0);

    sumPhaseSq += vPhase * vPhase;
    sumNeutralSq += vNeutral * vNeutral;
    sumVoltageSq += vAC * vAC;
    delay(1);
  }

  rmsPhase = sqrt(sumPhaseSq / numSamples) / acsSensitivity;
  rmsNeutral = sqrt(sumNeutralSq / numSamples) / acsSensitivity;
  voltageAC = sqrt(sumVoltageSq / numSamples) * voltageScalingFactor;

  theftDetected = abs(rmsPhase - rmsNeutral) > theftThreshold;
  if (theftDetected) {
    digitalWrite(relayPin, LOW);
    digitalWrite(buzzerPin, HIGH);
    lcd.clear();
    delay(500);
    lcd.setCursor(0, 0);
    lcd.print("THEFT DETECTED!");
    delay(9000);
  } else {
    digitalWrite(relayPin, HIGH);
    digitalWrite(buzzerPin, LOW);
  }
}

void updateLCD() {
  lcd.clear();
  if (theftDetected) {
    lcd.setCursor(0, 0);
    lcd.print("THEFT DETECTED!");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("V:");
    lcd.print(voltageAC, 1);
    lcd.print("V");
  }
  lcd.setCursor(0, 1);
  if (!theftDetected) {
    lcd.print("I_P:");
    lcd.print(rmsPhase, 2);
    lcd.print("I_N:");
    lcd.print(rmsNeutral, 2);
  }
}

void handleRoot() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html lang='en'>
<head>
<meta charset='UTF-8'>
<title>Electricity Theft Detection</title>
<script src='https://cdn.jsdelivr.net/npm/canvas-gauges@2.1.7/gauge.min.js'></script>
<style>
body {
  font-family:'Times New Roman',serif;
  text-align:center;
  margin:0;
  background: linear-gradient(270deg, #a8edea, #fed6e3);
  background-size: 400% 400%;
  animation: moveBG 10s ease infinite;
}
@keyframes moveBG { 0%{background-position:0% 50%;} 50%{background-position:100% 50%;} 100%{background-position:0% 50%;} }
h2 { color:#222; font-size:30px; margin-top:15px; }
.gauge-container { display:inline-block; margin:30px; transition:transform 0.3s; }
.gauge-container:hover { transform:scale(1.1); }
.gauge-title { font-size:20px; margin-top:10px; }
button {
  padding:12px 25px; font-size:18px; border:none; border-radius:10px; color:#fff; cursor:pointer;
  font-family:'Times New Roman',serif; transition:0.3s;
}
.on { background:#4CAF50; } .on:hover{background:#45a049;}
.off { background:#f44336; } .off:hover{background:#da190b;}
#theft { font-size:22px; font-weight:bold; margin-top:15px; }
.blink { animation: blinkAnim 1s infinite; }
@keyframes blinkAnim { 50% { background-color: rgba(255,0,0,0.3); } }
</style>
</head>
<body id='body'>
<h2>⚡ Electricity Monitoring and Theft Detection ⚡</h2>
<div class='gauge-container'>
<canvas id='phaseGauge' width='260' height='260'></canvas>
<div class='gauge-title'>Phase Current (A)</div></div>
<div class='gauge-container'>
<canvas id='neutralGauge' width='260' height='260'></canvas>
<div class='gauge-title'>Neutral Current (A)</div></div>
<div class='gauge-container'>
<canvas id='voltageGauge' width='260' height='260'></canvas>
<div class='gauge-title'>Voltage (V)</div></div>
<h3 id='theft'>Theft: NO</h3>
<button class='on' onclick='relayOn()'>Relay ON</button>
<button class='off' onclick='relayOff()'>Relay OFF</button>
<script>
var phaseGauge=new RadialGauge({renderTo:'phaseGauge',width:260,height:260,minValue:0,maxValue:1,units:'A',majorTicks:[0,0.2,0.4,0.5,0.6,0.8,1],highlights:[{from:0.7,to:1,color:'rgba(255,0,0,.6)'}],needleType:'arrow',needleWidth:3,animationDuration:500,borders:false,colorPlate:'#fff'}).draw();
var neutralGauge=new RadialGauge({renderTo:'neutralGauge',width:260,height:260,minValue:0,maxValue:1,units:'A',majorTicks:[0,0.2,0.4,0.5,0.6,0.8,1],highlights:[{from:0.7,to:1,color:'rgba(255,0,0,.6)'}],needleType:'arrow',needleWidth:3,animationDuration:500,borders:false,colorPlate:'#fff'}).draw();
var voltageGauge=new RadialGauge({renderTo:'voltageGauge',width:260,height:260,minValue:0,maxValue:300,units:'V',majorTicks:[0,50,100,150,200,230,250,300],highlights:[{from:230,to:300,color:'rgba(255,0,0,.6)'}],needleType:'arrow',needleWidth:3,animationDuration:500,borders:false,colorPlate:'#fff'}).draw();

let theftPopup=false;
async function updateData(){
  const r=await fetch('/data'); const j=await r.json();
  phaseGauge.value=j.rmsPhase; neutralGauge.value=j.rmsNeutral; voltageGauge.value=j.voltageAC;
  if(j.theft=="YES"){
    document.getElementById('theft').innerHTML="⚠ THEFT DETECTED ⚠";
    document.getElementById('theft').style.color="red";
    document.body.classList.add('blink');
    if(!theftPopup){alert("THEFT DETECTED! Immediate Action Required!"); theftPopup=true;}
  }else{
    document.getElementById('theft').innerHTML="Theft: NO";
    document.getElementById('theft').style.color="green";
    document.body.classList.remove('blink');
    theftPopup=false;
  }
  setTimeout(updateData,1000);
}
updateData();

function relayOn(){fetch('/relay/on');}
function relayOff(){fetch('/relay/off');}
</script>
</body></html>
)rawliteral";

  server.send(200, "text/html", page);
}

void sendData() {
  String json = "{\"rmsPhase\":" + String(rmsPhase, 2) +
                ",\"rmsNeutral\":" + String(rmsNeutral, 2) +
                ",\"voltageAC\":" + String(voltageAC, 1) +
                ",\"theft\":\"" + String(theftDetected ? "YES" : "NO") + "\"}";
  server.send(200, "application/json", json);
}
