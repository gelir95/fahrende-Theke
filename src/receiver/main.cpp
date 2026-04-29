#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Bluepad32.h>

#define Hupe 5

#define AP_SSID    "FahrendeTheke"
#define AP_PASS    "fahrende123"
#define AP_CHANNEL 1

// Sender switching
#define SENDER_TIMEOUT  300   // ms ohne Paket -> Sender-Lock freigeben

// Signal loss
#define SIGNAL_TIMEOUT  300   // ms ohne Paket -> Bremsen beginnen

// Ramp timing
#define RAMP_INTERVAL    20   // ms zwischen Ramp-Schritten

// Ramp step sizes (pro RAMP_INTERVAL, bezogen auf Max-Wert 2047)
// Höherer Wert = größerer Schritt pro Tick = schnellere Änderung
// Formel: ceil(2047 / STEP) * RAMP_INTERVAL = ms von Max auf 0
// Per Weboberfläche live einstellbar
int accelStep           = 80;    // Beschleunigung  (~520ms von 0 auf Max)
int decelStep           = 120;   // Verzögerung durch Joystick (~360ms von Max auf 0)
int signalLossDecelStep = 200;   // Verzögerung während Signalverlust (~220ms von Max auf 0)

// Notbremsung: Joystick >= X% in Gegenrichtung -> sofort auf 0
// Per Weboberfläche live einstellbar
float emergencyThreshold = 0.98f;

// Expo-Kurve (0.0 = linear, 1.0 = rein kubisch)
// Kleine Ausschläge -> wenig Reaktion, voller Ausschlag -> voller Wert
// Per Weboberfläche live einstellbar
float expoVR = 0.5f;
float expoRL = 0.5f;

// Totzone: Eingaben kleiner als X% des Maximalwerts werden als 0 behandelt
// Verhindert Controller-Drift bei losgelassenem Stick
// Per Weboberfläche live einstellbar
float deadzoneVR = 0.08f;   // 8% von 2047 = ~164
float deadzoneRL = 0.08f;   // 8% von 1300 = ~104

ControllerPtr myControllers[BP32_MAX_GAMEPADS];

unsigned long lastPacketMillis = 0;
unsigned long lastRampMillis   = 0;

uint8_t activeSenderMAC[6] = {0};
bool hasSender        = false;
bool waitForNeutralVR = false;

int targetVR  = 0;
int targetRL  = 0;
int currentVR = 0;
int currentRL = 0;

// Für Web-Telemetrie (werden im loop() aktualisiert)
bool liveSignalLost = false;
bool liveBtActive   = false;

typedef struct struct_message {
    int msg_vr;
    int msg_rl;
    bool hupe;
} struct_message;

struct_message myData;

// ---------------------------------------------------------------------------
// Weboberfläche
// ---------------------------------------------------------------------------

WebServer* server = nullptr;

static const char INDEX_HTML[] PROGMEM = R"html(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Fahrende Theke</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:monospace;background:#0d0d0d;color:#ccc;padding:16px;max-width:600px;margin:auto}
h1{color:#f0c040;margin-bottom:16px;font-size:1.3em}
h2{color:#40c0f0;font-size:.95em;margin:20px 0 10px;border-bottom:1px solid #333;padding-bottom:4px}
.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:6px;margin-bottom:4px}
.cell{background:#1a1a1a;border-radius:4px;padding:8px 4px;text-align:center}
.cell-label{font-size:.65em;color:#666;margin-bottom:3px}
.cell-val{font-size:1em;color:#40f0a0}
.dot{display:inline-block;width:10px;height:10px;border-radius:50%;background:#500}
.dot.on{background:#0f0}
.row{display:flex;align-items:center;gap:10px;margin:8px 0}
.row label{flex:1;font-size:.85em;color:#aaa}
.row input[type=number]{width:90px;background:#1a1a1a;color:#f0c040;border:1px solid #333;padding:5px 8px;font-family:monospace;font-size:.95em;border-radius:3px;text-align:right}
.row input[type=number]:focus{outline:none;border-color:#40c0f0}
.row .unit{flex:0 0 40px;font-size:.8em;color:#666}
.btns{margin-top:18px;display:flex;gap:10px}
button{background:#1a3a1a;color:#4f4;border:1px solid #4f4;padding:9px 20px;cursor:pointer;font-family:monospace;font-size:.9em;border-radius:3px}
button:hover{background:#2a5a2a}
button.sec{background:#1a1a3a;color:#88f;border-color:#88f}
button.sec:hover{background:#2a2a5a}
#msg{margin-top:10px;font-size:.8em;color:#8f8;min-height:1.2em}
</style>
</head>
<body>
<h1>Fahrende Theke</h1>

<h2>Live-Status</h2>
<div class="grid">
  <div class="cell"><div class="cell-label">VR aktuell</div><div class="cell-val" id="s-vr">-</div></div>
  <div class="cell"><div class="cell-label">RL aktuell</div><div class="cell-val" id="s-rl">-</div></div>
  <div class="cell"><div class="cell-label">VR Ziel</div><div class="cell-val" id="s-tvr">-</div></div>
  <div class="cell"><div class="cell-label">RL Ziel</div><div class="cell-val" id="s-trl">-</div></div>
  <div class="cell"><div class="cell-label">Bluetooth</div><div class="cell-val"><span class="dot" id="s-bt"></span></div></div>
  <div class="cell"><div class="cell-label">ESP-NOW</div><div class="cell-val"><span class="dot" id="s-espnow"></span></div></div>
  <div class="cell"><div class="cell-label">Signal OK</div><div class="cell-val"><span class="dot on" id="s-sig"></span></div></div>
  <div class="cell"><div class="cell-label">Notbremse</div><div class="cell-val"><span class="dot" id="s-emb"></span></div></div>
</div>

<h2>Rampe &amp; Beschleunigung</h2>
<div class="row"><label>Beschleunigung</label><input type="number" id="accelStep" min="1" max="400" step="1" value="80"><span class="unit">1–400</span></div>
<div class="row"><label>Verzögerung Joystick</label><input type="number" id="decelStep" min="1" max="400" step="1" value="120"><span class="unit">1–400</span></div>
<div class="row"><label>Verzögerung Signalverlust</label><input type="number" id="signalLossDecelStep" min="1" max="600" step="1" value="200"><span class="unit">1–600</span></div>

<h2>Expo-Kurve &amp; Totzone</h2>
<div class="row"><label>Expo VR (0=linear, 1=kubisch)</label><input type="number" id="expoVR" min="0" max="1" step="0.01" value="0.5"><span class="unit">0–1</span></div>
<div class="row"><label>Expo RL</label><input type="number" id="expoRL" min="0" max="1" step="0.01" value="0.5"><span class="unit">0–1</span></div>
<div class="row"><label>Totzone VR</label><input type="number" id="deadzoneVR" min="0" max="0.3" step="0.005" value="0.08"><span class="unit">0–0.3</span></div>
<div class="row"><label>Totzone RL</label><input type="number" id="deadzoneRL" min="0" max="0.3" step="0.005" value="0.08"><span class="unit">0–0.3</span></div>

<h2>Notbremsung</h2>
<div class="row"><label>Schwelle Gegenrichtung</label><input type="number" id="emergencyThreshold" min="0.5" max="1" step="0.01" value="0.98"><span class="unit">0.5–1</span></div>

<div class="btns">
  <button onclick="apply()">Übernehmen</button>
  <button class="sec" onclick="pull()">Vom Gerät laden</button>
</div>
<div id="msg"></div>

<script>
const PARAMS=['accelStep','decelStep','signalLossDecelStep','emergencyThreshold','expoVR','expoRL','deadzoneVR','deadzoneRL'];
function dot(id,on){document.getElementById(id).className='dot'+(on?' on':'');}
function pollStatus(){
  fetch('/status').then(r=>r.json()).then(d=>{
    document.getElementById('s-vr').textContent=d.currentVR;
    document.getElementById('s-rl').textContent=d.currentRL;
    document.getElementById('s-tvr').textContent=d.targetVR;
    document.getElementById('s-trl').textContent=d.targetRL;
    dot('s-bt',d.btActive);
    dot('s-espnow',d.hasSender);
    dot('s-sig',!d.signalLost);
    dot('s-emb',d.waitForNeutralVR);
  }).catch(()=>{});
}
function pull(){
  fetch('/status').then(r=>r.json()).then(d=>{
    PARAMS.forEach(k=>{const el=document.getElementById(k);if(el&&d[k]!==undefined)el.value=d[k];});
  }).catch(()=>{});
}
function apply(){
  const p=new URLSearchParams();
  PARAMS.forEach(k=>p.append(k,document.getElementById(k).value));
  fetch('/set?'+p.toString()).then(()=>{
    const m=document.getElementById('msg');
    m.textContent='Übernommen ✓';
    setTimeout(()=>m.textContent='',2000);
  });
}
setInterval(pollStatus,500);
pull();
</script>
</body>
</html>)html";

void handleRoot() {
  server->send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
  char buf[400];
  snprintf(buf, sizeof(buf),
    "{\"currentVR\":%d,\"currentRL\":%d,\"targetVR\":%d,\"targetRL\":%d,"
    "\"btActive\":%s,\"hasSender\":%s,\"signalLost\":%s,\"waitForNeutralVR\":%s,"
    "\"accelStep\":%d,\"decelStep\":%d,\"signalLossDecelStep\":%d,"
    "\"emergencyThreshold\":%.3f,\"expoVR\":%.3f,\"expoRL\":%.3f,"
    "\"deadzoneVR\":%.3f,\"deadzoneRL\":%.3f}",
    currentVR, currentRL, targetVR, targetRL,
    liveBtActive     ? "true" : "false",
    hasSender        ? "true" : "false",
    liveSignalLost   ? "true" : "false",
    waitForNeutralVR ? "true" : "false",
    accelStep, decelStep, signalLossDecelStep,
    emergencyThreshold, expoVR, expoRL, deadzoneVR, deadzoneRL
  );
  server->send(200, "application/json", buf);
}

void handleSet() {
  if (server->hasArg("accelStep"))           accelStep           = server->arg("accelStep").toInt();
  if (server->hasArg("decelStep"))           decelStep           = server->arg("decelStep").toInt();
  if (server->hasArg("signalLossDecelStep")) signalLossDecelStep = server->arg("signalLossDecelStep").toInt();
  if (server->hasArg("emergencyThreshold"))  emergencyThreshold  = server->arg("emergencyThreshold").toFloat();
  if (server->hasArg("expoVR"))              expoVR              = server->arg("expoVR").toFloat();
  if (server->hasArg("expoRL"))              expoRL              = server->arg("expoRL").toFloat();
  if (server->hasArg("deadzoneVR"))          deadzoneVR          = server->arg("deadzoneVR").toFloat();
  if (server->hasArg("deadzoneRL"))          deadzoneRL          = server->arg("deadzoneRL").toFloat();
  server->send(200, "text/plain", "OK");
}

// ---------------------------------------------------------------------------

void onConnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == nullptr) {
            myControllers[i] = ctl;
            return;
        }
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == ctl) {
            myControllers[i] = nullptr;
            return;
        }
    }
}

bool macEqual(const uint8_t *a, const uint8_t *b) {
  for (int i = 0; i < 6; i++) if (a[i] != b[i]) return false;
  return true;
}

// Expo-Kurve: output = expo * x^3 + (1 - expo) * x  (normalisiert auf maxVal)
// Totzone: Eingaben innerhalb der Deadzone werden als 0 behandelt
int applyExpo(int input, int maxVal, float expo, float deadzone) {
  if (abs(input) < (int)(deadzone * maxVal)) return 0;
  float norm   = (float)input / (float)maxVal;
  float curved = expo * norm * norm * norm + (1.0f - expo) * norm;
  return (int)(curved * (float)maxVal);
}

// Einen Schritt von current Richtung target, mit unterschiedlichen Raten für Accel/Decel
int rampToward(int current, int target, int aStep, int dStep) {
  int diff = target - current;
  if (diff == 0) return current;

  // Weg von 0 = beschleunigen, Richtung 0 = verzögern
  bool accelerating = (diff > 0 && current >= 0) || (diff < 0 && current <= 0);
  int step = accelerating ? aStep : dStep;

  if (diff >  step) return current + step;
  if (diff < -step) return current - step;
  return target;
}

int rampToZero(int val, int step) {
  if (val >  step) return val - step;
  if (val < -step) return val + step;
  return 0;
}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  unsigned long now = millis();
  const uint8_t *mac = recv_info->src_addr;

  // Sender-Lock: kein aktiver Sender oder Timeout -> neuen Sender annehmen
  if (!hasSender || (now - lastPacketMillis > SENDER_TIMEOUT)) {
    memcpy(activeSenderMAC, mac, 6);
    hasSender = true;
  }

  // Pakete von anderen Sendern ignorieren
  if (!macEqual(mac, activeSenderMAC)) return;

  memcpy(&myData, incomingData, sizeof(myData));
  lastPacketMillis = now;
}

void setup() {
  Serial.begin(9600);

  // AP+STA: ESP-NOW auf STA, Webserver auf SoftAP (192.168.4.1)
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS, AP_CHANNEL);
  server = new WebServer(80);
  server->on("/",       HTTP_GET, handleRoot);
  server->on("/status", HTTP_GET, handleStatus);
  server->on("/set",    HTTP_GET, handleSet);
  server->begin();
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  delay(1000);
  Serial.println("VR:startup");
  Serial.println("RL:startup");

  if (esp_now_init() != ESP_OK) return;
  esp_now_register_recv_cb(OnDataRecv);

  BP32.setup(&onConnectedController, &onDisconnectedController);

  pinMode(Hupe, OUTPUT);
  delay(200);
}

void loop() {
  server->handleClient();
  BP32.update();
  unsigned long now = millis();

  // Eingabequelle: Bluetooth-Controller hat Vorrang vor ESP-NOW
  bool btActive = false;
  for (auto ctl : myControllers) {
    if (ctl && ctl->isConnected()) {
      btActive = true;
      // Linker Stick Y -> VR (invertiert: Stick vorwärts = positiver Wert)
      int rawVR = map(ctl->axisY(), -512, 511, 2047, -2047);
      // Linker Stick X -> RL
      int rawRL = map(ctl->axisX(), -512, 511, -1300, 1300);
      targetVR = applyExpo(rawVR, 2047, expoVR, deadzoneVR);
      targetRL = applyExpo(rawRL, 1300, expoRL, deadzoneRL);
      // A-Taste -> Hupe
      digitalWrite(Hupe, ctl->a() ? HIGH : LOW);
      break;
    }
  }
  liveBtActive = btActive;

  bool signalLost = false;
  if (!btActive) {
    signalLost = (now - lastPacketMillis >= SIGNAL_TIMEOUT);
    if (!signalLost) {
      // Expo anwenden -> wird Zielwert für den Ramp
      targetVR = applyExpo(myData.msg_vr, 2047, expoVR, deadzoneVR);

      // RL mit Hysterese: aktiviert erst bei 130% der Deadzone, deaktiviert bei 100%
      static bool rlActive = false;
      int rlRaw = abs(myData.msg_rl);
      int rlDeadLow  = (int)(deadzoneRL * 1300);
      int rlDeadHigh = (int)(deadzoneRL * 1300 * 1.3f);
      if (!rlActive && rlRaw >= rlDeadHigh) rlActive = true;
      if ( rlActive && rlRaw <  rlDeadLow)  rlActive = false;
      targetRL = rlActive ? applyExpo(myData.msg_rl, 1300, expoRL, deadzoneRL) : 0;
      digitalWrite(Hupe, myData.hupe ? HIGH : LOW);
    }
  }
  liveSignalLost = signalLost;

  if (signalLost) {
    digitalWrite(Hupe, LOW);
    targetVR = 0;
    targetRL = 0;
  }

  if (now - lastRampMillis >= RAMP_INTERVAL) {
    lastRampMillis = now;

    if (signalLost) {
      currentVR = rampToZero(currentVR, signalLossDecelStep);
      currentRL = 0;
    } else {
      // Notbremsung: Joystick >= EMERGENCY_THRESHOLD in Gegenrichtung -> sofort auf 0
      bool emBrakeVR = (currentVR != 0)
                    && (abs(targetVR) >= (int)(emergencyThreshold * 2047))
                    && ((targetVR >= 0) != (currentVR >= 0));

      if (emBrakeVR) {
        currentVR = 0;
        waitForNeutralVR = true;
      }

      // VR: erst wieder fahren wenn Joystick auf 0 war (verhindert versehentliches Rückwärtsfahren)
      if (waitForNeutralVR) {
        if (targetVR == 0) waitForNeutralVR = false;
      } else if (!emBrakeVR) {
        // VR: normales Fahren/Beschleunigen per Ramp
        currentVR = rampToward(currentVR, targetVR, accelStep, decelStep);
      }

      // RL: direkte Übertragung (Lenken braucht sofortige Reaktion)
      currentRL = targetRL;
    }

    Serial.print("VR:");
    Serial.println(currentVR);
    Serial.print("RL:");
    Serial.println(currentRL);
  }
}
