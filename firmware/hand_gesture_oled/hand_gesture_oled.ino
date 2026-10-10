/*
 * Hand Gesture -> ESP32 OLED
 * --------------------------
 * Your phone's browser runs Google MediaPipe HandLandmarker (in-browser, on the
 * device camera), detects which fingers are folded/extended, and POSTs the
 * 5-finger mask to the ESP32 over WiFi. The ESP32 draws a schematic hand
 * (folded vs extended) + the gesture name on a 128x64 SSD1306 OLED.
 *
 * The ESP32 runs in STATION mode (joins your WiFi) so the phone - on the same
 * WiFi - has the internet MediaPipe's model needs. The OLED shows the ESP32's
 * IP on boot; open http://<that-ip> on the phone.
 *
 * Camera over http needs a "secure context", so on the phone's Chrome:
 *   chrome://flags -> "Insecure origins treated as secure" -> add http://<that-ip> -> relaunch.
 *
 * 1. Fill WIFI_SSID / WIFI_PASS below.
 * 2. Libraries: Adafruit GFX, Adafruit SSD1306.
 * Wiring (I2C):  ESP32 3V3->VCC  GND->GND  GPIO21->SDA  GPIO22->SCL
 * License: MIT.
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---- configure me ----
#define WIFI_SSID "YOUR_WIFI"
#define WIFI_PASS "YOUR_PASS"
// ----------------------

Adafruit_SSD1306 display(128, 64, &Wire, -1);
WebServer server(80);
uint8_t addr = 0x3C;

bool fT = false, fI = false, fM = false, fR = false, fP = false;
int fcount = 0;
String gname = "--";

void drawHandIcon(int ox) {
  display.fillRoundRect(ox + 4, 42, 34, 20, 4, SSD1306_WHITE);     // palm
  int fx[4] = { ox + 8, ox + 16, ox + 24, ox + 32 };              // index..pinky
  bool fu[4] = { fI, fM, fR, fP };
  for (int k = 0; k < 4; k++) {
    if (fu[k]) display.fillRoundRect(fx[k], 14, 5, 30, 2, SSD1306_WHITE);   // extended
    else       display.fillRoundRect(fx[k], 36, 5, 8, 2, SSD1306_WHITE);    // folded stub
  }
  if (fT) display.fillRoundRect(ox - 8, 44, 14, 5, 2, SSD1306_WHITE);        // thumb out
  else    display.fillRoundRect(ox, 47, 8, 5, 2, SSD1306_WHITE);             // thumb tucked
}

void drawScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  if (gname == "NO HAND" || gname == "--") {
    display.setTextSize(1); display.setCursor(0, 0); display.print("HAND GESTURE");
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
    display.setTextSize(2); display.setCursor(18, 30); display.print("NO HAND");
    display.display();
    return;
  }
  drawHandIcon(12);
  display.drawFastVLine(62, 0, 64, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(70, 4);  display.print("GESTURE");
  display.setCursor(70, 18); display.print(gname);
  display.setTextSize(3);
  display.setCursor(74, 36); display.print(fcount);
  display.setTextSize(1);
  display.setCursor(96, 52); display.print("/5");
  display.display();
}

const char PAGE[] PROGMEM = R"HTML(<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Hand Gesture</title><style>
*{box-sizing:border-box}body{margin:0;background:#0b0f14;color:#e8f0f8;font-family:system-ui,Arial;overflow:hidden}
#wrap{position:relative;width:100vw;max-width:520px;margin:0 auto}
video{display:none}canvas#disp{width:100%;display:block;background:#000}
.bar{position:absolute;left:0;right:0;padding:7px 10px;font-size:13px;display:flex;justify-content:space-between;align-items:center;background:rgba(12,16,22,.55)}
.top{top:0}.bot{bottom:0}.title{color:#ff78ff;font-weight:700;letter-spacing:1px}.live{color:#5de28a}.g{font-size:20px;font-weight:700;color:#ffd23c}
#start{position:absolute;inset:0;display:flex;flex-direction:column;align-items:center;justify-content:center;background:#0b0f14;gap:16px;padding:20px}
button{padding:14px 22px;background:#13303a;color:#2ee6f6;border:1px solid #2ee6f6;border-radius:10px;font-size:16px}
.hint{font-size:12px;color:#8aa;max-width:320px;text-align:center;line-height:1.6}
#flip{position:absolute;right:10px;bottom:48px;padding:6px 10px;font-size:12px;background:rgba(26,36,48,.85);border:1px solid #334;color:#def;border-radius:8px}
#msg{position:absolute;left:10px;bottom:48px;font-size:11px;color:#8aa}
</style></head><body>
<div id="wrap">
<video id="v" playsinline muted></video>
<canvas id="disp" width="480" height="360"></canvas>
<div class="bar top"><span class="title">HAND GESTURE</span><span class="live">&#9679; LIVE</span></div>
<div class="bar bot"><span id="fing">FINGERS 0/5</span><span class="g" id="ges">--</span><span id="mask" style="color:#8aa">-----</span></div>
<button id="flip">flip cam</button><div id="msg"></div>
<div id="start">
  <b class="title" style="font-size:18px">HAND GESTURE &rarr; OLED</b>
  <button id="go">Start</button>
  <div class="hint">Loading MediaPipe... point the camera at your hand - each finger is detected separately.<br>(needs internet + the camera flag)</div>
</div>
</div>
<script type="module">
import {HandLandmarker, FilesetResolver, DrawingUtils} from "https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@0.10.14/vision_bundle.mjs";
const v=document.getElementById('v'),disp=document.getElementById('disp'),dc=disp.getContext('2d');
const fingEl=document.getElementById('fing'),gesEl=document.getElementById('ges'),maskEl=document.getElementById('mask'),msg=document.getElementById('msg');
const du=new DrawingUtils(dc);
let hl=null,facing='environment',running=false,lastSent=0,pend='',pendT=0,stable='';
const NAMES={'00000':'FIST','11111':'OPEN','01000':'POINT','01100':'PEACE','10000':'THUMBSUP','01001':'ROCK','10001':'CALL','11000':'GUN','00001':'PINKY','00100':'MIDDLE','01110':'THREE','11100':'THREE'};

const initMP=async()=>{
  msg.textContent='loading model...';
  const vis=await FilesetResolver.forVisionTasks("https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@0.10.14/wasm");
  hl=await HandLandmarker.createFromOptions(vis,{baseOptions:{modelAssetPath:"https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task",delegate:"GPU"},runningMode:"VIDEO",numHands:1});
  msg.textContent='';
}
const fingersUp=(lm)=>{
  const up=[];
  const D=(a,b)=>Math.hypot(lm[a].x-lm[b].x,lm[a].y-lm[b].y);
  up.push(D(4,17)>D(3,17)?1:0);   // thumb (geometry-based, handedness-independent)
  [[8,6],[12,10],[16,14],[20,18]].forEach(p=>up.push(lm[p[0]].y<lm[p[1]].y?1:0));
  return up;
}
const send=(m,n,c)=>{fetch('/g?m='+m+'&n='+n+'&c='+c).catch(()=>{});lastSent=performance.now();};

const start=async()=>{
  try{
    if(!hl)await initMP();
    const s=await navigator.mediaDevices.getUserMedia({video:{facingMode:facing},audio:false});
    v.srcObject=s;await v.play();running=true;
    document.getElementById('start').style.display='none';requestAnimationFrame(loop);
  }catch(e){ alert('Error: '+e.message+'\n\nFor camera over http, enable the Chrome flag:\nchrome://flags -> "Insecure origins treated as secure" -> add this page URL -> relaunch.\nKeep internet on (MediaPipe model loads from a CDN).'); }
}

let lastVideoT=-1;
const loop=()=>{
  if(!running)return;
  const DW=disp.width,DH=disp.height;
  dc.drawImage(v,0,0,DW,DH);
  let mask='-----',name='NO HAND',count=0;
  if(v.currentTime!==lastVideoT){
    lastVideoT=v.currentTime;
    const res=hl.detectForVideo(v,performance.now());
    if(res.landmarks&&res.landmarks.length){
      const lm=res.landmarks[0];
      du.drawConnectors(lm,HandLandmarker.HAND_CONNECTIONS,{color:'#ff78ff',lineWidth:3});
      du.drawLandmarks(lm,{color:'#ffd23c',radius:3});
      const up=fingersUp(lm);
      mask=up.join('');count=up.reduce((a,b)=>a+b,0);
      name=NAMES[mask]||(count+' UP');
    }
  }
  const now=performance.now();
  if(mask!==stable){
    if(pend!==mask){pend=mask;pendT=now;}
    else if(now-pendT>=120){stable=mask;send(mask,name,count);}
  }else pend=mask;
  if(now-lastSent>500&&stable==='-----'&&mask==='-----'){send('-----','NO HAND',0);}

  fingEl.textContent='FINGERS '+count+'/5';gesEl.textContent=name;maskEl.textContent=mask;
  requestAnimationFrame(loop);
}
document.getElementById('go').onclick=start;
document.getElementById('flip').onclick=async()=>{facing=(facing==='environment'?'user':'environment');if(v.srcObject)v.srcObject.getTracks().forEach(t=>t.stop());running=false;await start();};
initMP();
</script></body></html>)HTML";

void handleRoot() { server.send_P(200, "text/html", PAGE); }

void handleG() {
  String m = server.arg("m");
  if (m.length() == 5) { fT = m[0] == '1'; fI = m[1] == '1'; fM = m[2] == '1'; fR = m[3] == '1'; fP = m[4] == '1'; }
  if (server.hasArg("c")) fcount = server.arg("c").toInt();
  if (server.hasArg("n")) gname = server.arg("n");
  drawScreen();
  server.send(200, "text/plain", "ok");
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); Wire.setClock(400000);
  for (uint8_t a = 1; a < 127; a++) { Wire.beginTransmission(a); if (Wire.endTransmission() == 0 && (a == 0x3C || a == 0x3D)) addr = a; }
  display.begin(SSD1306_SWITCHCAPVCC, addr);
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0, 0); display.println("Connecting WiFi...");
  display.display();

  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) { delay(300); }

  display.clearDisplay(); display.setCursor(0, 0);
  if (WiFi.status() == WL_CONNECTED) {
    String ip = WiFi.localIP().toString();
    Serial.printf("[sta] ip=%s\n", ip.c_str());
    display.setTextSize(1); display.println("HAND GESTURE");
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
    display.setCursor(0, 16); display.print("IP: "); display.println(ip);
    display.setCursor(0, 32); display.println("phone: same WiFi");
    display.setCursor(0, 44); display.print("open http://");
    display.setCursor(0, 54); display.print(ip);
  } else {
    display.setTextSize(2); display.println("WiFi FAIL");
    display.setTextSize(1); display.println("check SSID/PASS");
  }
  display.display();

  server.on("/", handleRoot);
  server.on("/g", handleG);
  server.begin();
}

void loop() { server.handleClient(); }
