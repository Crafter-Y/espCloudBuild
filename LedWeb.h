// LedWeb.h
#pragma once
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <FastLED.h>

// ======= Public API =======
namespace LedWeb {
  // Du musst diese vor begin() aufrufen:
  void attachLayout(
    CRGB* trunk, uint16_t trunkLen,
    CRGB* L1, CRGB* L2, CRGB* L3,
    CRGB* R1, CRGB* R2, CRGB* R3,
    const uint16_t* Y_LEFT, const uint16_t* Y_RIGHT,
    const bool* L_REVERSED, const bool* R_REVERSED, uint8_t branchLen
  );

  // Startet AP + Webserver (Passwort mind. 8 Zeichen!)
  void begin(const char* ssid = "Barschrank", const char* password = "12345678");

  // Setze/gette Status, damit Webseite mitreden darf:
  void setBrightnessSetter(void (*fn)(uint8_t));   // ruft deine setBrightness()
  void setPowerSetter(void (*fn)(bool));           // ruft dein Power ON/OFF

  void setStatusGetter(uint8_t (*getBright)(), bool (*getPower)());

  // Call aus deinem Render-Loop, direkt NACH FastLED.show():
  void broadcastFrame();    // pusht aktuelle Farben an die Clients
}

// ======= Implementation =======
namespace LedWeb {
  // Layout-Pointer
  static CRGB *TRUNKp=nullptr, *L1p=nullptr, *L2p=nullptr, *L3p=nullptr, *R1p=nullptr, *R2p=nullptr, *R3p=nullptr;
  static uint16_t TRUNK_LEN=0, BRANCH_LEN=0;
  static const uint16_t *Y_LEFT=nullptr, *Y_RIGHT=nullptr;
  static const bool *L_REV=nullptr, *R_REV=nullptr;

  // Setter/Getter Hooks (implementiert im Sketch)
  static void (*_setBrightness)(uint8_t)=nullptr;
  static void (*_setPower)(bool)=nullptr;
  static uint8_t (*_getBrightness)()=nullptr;
  static bool (*_getPower)()=nullptr;

  // Server
  static AsyncWebServer server(80);
  static AsyncWebSocket ws("/ws");

  // ---- kleine Helper ----
  static String rgbToHex(const CRGB& c){
    char buf[8];
    sprintf(buf, "#%02X%02X%02X", c.r, c.g, c.b);
    return String(buf);
  }

  // Baut die Frame-JSON (Farben + Status)
  static String buildFrameJson(){
    // 305 Pixel => JSON passt locker in ~10–20 KB
    DynamicJsonDocument doc(24*1024);
    doc["type"] = "frame";
    if (_getBrightness) doc["b"] = _getBrightness();
    if (_getPower)      doc["on"]= _getPower();

    JsonArray colors = doc.createNestedArray("colors");
    // Reihenfolge: TRUNK, L1, L2, L3, R1, R2, R3 (in Array-Reihenfolge)
    for(uint16_t i=0;i<TRUNK_LEN;i++) colors.add(rgbToHex(TRUNKp[i]));
    CRGB* branches[6] = {L1p,L2p,L3p,R1p,R2p,R3p};
    for(int bi=0;bi<6;bi++){
      for(uint16_t i=0;i<BRANCH_LEN;i++) colors.add(rgbToHex(branches[bi][i]));
    }

    // Meta für die Zeichnung
    doc["meta"]["trunkLen"] = TRUNK_LEN;
    doc["meta"]["branchLen"]= BRANCH_LEN;
    JsonArray yl = doc["meta"].createNestedArray("Y_LEFT");
    JsonArray yr = doc["meta"].createNestedArray("Y_RIGHT");
    for(int i=0;i<3;i++){ yl.add(Y_LEFT[i]); yr.add(Y_RIGHT[i]); }
    JsonArray lrev = doc["meta"].createNestedArray("L_REV");
    JsonArray rrev = doc["meta"].createNestedArray("R_REV");
    for(int i=0;i<3;i++){ lrev.add(L_REV[i]); rrev.add(R_REV[i]); }

    String out; serializeJson(doc, out);
    return out;
  }

  // ---- WebSocket Events ----
  static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type,
                        void* arg, uint8_t* data, size_t len) {
    if(type == WS_EVT_CONNECT){
      // Beim Connect sofort aktuellen Frame schicken:
      String msg = buildFrameJson();
      client->text(msg);
    } else if(type == WS_EVT_DATA){
      // Client → Server commands (JSON)
      AwsFrameInfo *info = (AwsFrameInfo*)arg;
      if(info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT){
        String s((char*)data, len);
        DynamicJsonDocument doc(2048);
        DeserializationError err = deserializeJson(doc, s);
        if(err) return;
        const char* t = doc["type"] | "";
        if (strcmp(t,"set")==0){
          if (doc.containsKey("on") && _setPower)       _setPower((bool)doc["on"]);
          if (doc.containsKey("b")  && _setBrightness)  _setBrightness((uint8_t)doc["b"]);
          // nach Änderung Status zurücksenden
          server->textAll(buildFrameJson());
        }
      }
    }
  }

  // ---- HTTP Handlers ----
  // Minimal-UI (Pixelansicht + Toggle + Slider), inline im PROGMEM
  static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Barschrank LEDs</title>
<style>
  body{font-family:system-ui,Segoe UI,Arial;margin:16px}
  #canvas{border:1px solid #ccc; image-rendering: pixelated;}
  .row{display:flex;gap:16px;align-items:center;margin:8px 0}
  .btn{padding:8px 12px;border:1px solid #888;border-radius:8px;cursor:pointer;background:#f3f3f3}
  .on{background:#c7ffd1}
  .label{min-width:90px}
  input[type=range]{width:220px}
</style></head>
<body>
  <h2>Barschrank LEDs</h2>
  <div class="row">
    <span class="label">Status</span>
    <button id="power" class="btn">Aus</button>
    <span id="bVal"></span>
  </div>
  <div class="row">
    <span class="label">Helligkeit</span>
    <input type="range" id="bRange" min="26" max="230" value="128">
  </div>
  <canvas id="canvas" width="600" height="1400"></canvas>

<script>
let ws;
let meta=null;
const canvas = document.getElementById('canvas');
const ctx = canvas.getContext('2d');
const powerBtn = document.getElementById('power');
const bRange = document.getElementById('bRange');
const bVal = document.getElementById('bVal');

function connect(){
  ws = new WebSocket(`ws://${location.host}/ws`);
  ws.onopen = () => console.log('ws open');
  ws.onmessage = ev => {
    const msg = JSON.parse(ev.data);
    if(msg.type === 'frame'){
      if(!meta) meta = msg.meta;
      render(msg);
      powerBtn.classList.toggle('on', !!msg.on);
      powerBtn.textContent = msg.on ? 'An' : 'Aus';
      if(typeof msg.b === 'number'){ bRange.value = msg.b; bVal.textContent = `(${msg.b})`; }
    }
  };
  ws.onclose = () => setTimeout(connect, 1000);
}

function sendSet(obj){
  if(ws && ws.readyState === 1){
    ws.send(JSON.stringify({type:'set', ...obj}));
  }
}

powerBtn.onclick = () => {
  const isOn = powerBtn.classList.contains('on');
  sendSet({on: !isOn});
};

bRange.oninput = () => {
  bVal.textContent = `(${bRange.value})`;
  sendSet({b: Number(bRange.value)});
};

function hexToRGB(hex){
  const m = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
  return m ? {r:parseInt(m[1],16), g:parseInt(m[2],16), b:parseInt(m[3],16)} : {r:0,g:0,b:0};
}

function drawPix(x,y,hex,size){
  const c = hexToRGB(hex);
  ctx.fillStyle = hex;
  ctx.fillRect(x,y,size,size);
  // optional: Rahmen
  ctx.strokeStyle = 'rgba(0,0,0,0.1)';
  ctx.strokeRect(x,y,size,size);
}

function render(msg){
  const trunkLen = meta.trunkLen;
  const blen = meta.branchLen;
  const YL = meta.Y_LEFT, YR = meta.Y_RIGHT;
  const LREV = meta.L_REV, RREV = meta.R_REV;

  const colors = msg.colors; // Reihenfolge: trunk, L1,L2,L3,R1,R2,R3
  ctx.clearRect(0,0,canvas.width,canvas.height);

  const px = 6;        // Pixelgröße (Canvas)
  const gap = 2;       // Abstand
  const colCenter = Math.floor(canvas.width/2);
  const trunkX = colCenter - Math.floor(px/2);

  // Stamm vertikal malen (y=0 unten -> wir drehen, damit y=0 oben schöner aussieht)
  for(let y=0;y<trunkLen;y++){
    const ix = y;
    const c = colors[ix];
    drawPix(trunkX, y*(px+gap), c, px);
  }

  // Hilfsfunktion: Ast zeichnen
  function drawBranch(anchorY, isRight, rev, arrStart){
    // arrStart ist Index im colors[]-Array
    for (let i=0;i<blen;i++){
      const idx = arrStart + i;
      const drawIndex = rev ? (blen-1-i) : i; // außen -> innen
      const xOff = (drawIndex+1)*(px+gap);
      const x = isRight ? (trunkX + px + gap + xOff) : (trunkX - gap - xOff - px);
      const y = anchorY*(px+gap);
      drawPix(x, y, colors[idx], px);
    }
  }

  let base = trunkLen;     // Startindex der Branch-Farben
  // L1,L2,L3
  drawBranch(YL[0], false, LREV[0], base);         base += blen;
  drawBranch(YL[1], false, LREV[1], base);         base += blen;
  drawBranch(YL[2], false, LREV[2], base);         base += blen;
  // R1,R2,R3
  drawBranch(YR[0], true,  RREV[0], base);         base += blen;
  drawBranch(YR[1], true,  RREV[1], base);         base += blen;
  drawBranch(YR[2], true,  RREV[2], base);
}

connect();
</script>
</body></html>
)HTML";

  // ====== Public impl ======
  void attachLayout(
    CRGB* trunk, uint16_t trunkLen,
    CRGB* L1, CRGB* L2, CRGB* L3,
    CRGB* R1, CRGB* R2, CRGB* R3,
    const uint16_t* yl, const uint16_t* yr,
    const bool* lrev, const bool* rrev, uint8_t branchLen
  ){
    TRUNKp = trunk; TRUNK_LEN = trunkLen;
    L1p=L1; L2p=L2; L3p=L3; R1p=R1; R2p=R2; R3p=R3;
    Y_LEFT = yl; Y_RIGHT = yr;
    L_REV = lrev; R_REV = rrev;
    BRANCH_LEN = branchLen;
  }

  void setBrightnessSetter(void (*fn)(uint8_t)){ _setBrightness = fn; }
  void setPowerSetter(void (*fn)(bool)){ _setPower = fn; }
  void setStatusGetter(uint8_t (*getBright)(), bool (*getPower)()){
    _getBrightness = getBright; _getPower = getPower;
  }

  void begin(const char* ssid, const char* password){
    // SoftAP (WPA2: min. 8 Zeichen Kennwort)
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // GET /
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req){
      req->send_P(200, "text/html; charset=utf-8", INDEX_HTML);
    });

    // REST Fallbacks (optional, nützlich für Tests):
    server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* req){
      String s = buildFrameJson();
      req->send(200, "application/json", s);
    });
    server.on("/api/power", HTTP_POST, [](AsyncWebServerRequest* req){
      if (!req->hasParam("on", true)) { req->send(400, "text/plain", "missing on=0/1"); return; }
      bool on = req->getParam("on", true)->value().toInt() != 0;
      if (_setPower) _setPower(on);
      ws.textAll(buildFrameJson());
      req->send(200, "text/plain", "ok");
    });
    server.on("/api/brightness", HTTP_POST, [](AsyncWebServerRequest* req){
      if (!req->hasParam("b", true)) { req->send(400, "text/plain", "missing b=0..255"); return; }
      uint8_t b = (uint8_t) req->getParam("b", true)->value().toInt();
      if (_setBrightness) _setBrightness(b);
      ws.textAll(buildFrameJson());
      req->send(200, "text/plain", "ok");
    });

    server.begin();
  }

  void broadcastFrame(){
    ws.textAll(buildFrameJson());
  }
}
