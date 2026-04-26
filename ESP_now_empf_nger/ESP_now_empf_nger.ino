/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp-now-esp32-arduino-ide/

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

#include <esp_now.h>
#include <WiFi.h>

#define Hupe 5

// Sender switching
#define SENDER_TIMEOUT  300   // ms ohne Paket -> Sender-Lock freigeben

// Signal loss
#define SIGNAL_TIMEOUT  200   // ms ohne Paket -> Bremsen beginnen

// Ramp timing
#define RAMP_INTERVAL    20   // ms zwischen Ramp-Schritten

// Ramp step sizes (pro RAMP_INTERVAL, bezogen auf Max-Wert 2047)
// Höherer Wert = größerer Schritt pro Tick = schnellere Änderung
// Formel: ceil(2047 / STEP) * RAMP_INTERVAL = ms von Max auf 0
#define ACCEL_STEP      80    // Beschleunigung  (~520ms von 0 auf Max)
#define DECEL_STEP      120   // Verzögerung durch Joystick (~360ms von Max auf 0)
#define SIGNAL_LOSS_DECEL_STEP 200  // Verzögerung während Signalverlust (~220ms von Max auf 0)

// Notbremsung: Joystick >= X% in Gegenrichtung -> sofort auf 0
#define EMERGENCY_THRESHOLD 0.98f

// Expo-Kurve (0.0 = linear, 1.0 = rein kubisch)
// Kleine Ausschläge -> wenig Reaktion, voller Ausschlag -> voller Wert
#define EXPO_VR         0.5f
#define EXPO_RL         0.5f

unsigned long lastPacketMillis = 0;
unsigned long lastRampMillis   = 0;

uint8_t activeSenderMAC[6] = {0};
bool hasSender = false;

int targetVR  = 0;
int targetRL  = 0;
int currentVR = 0;
int currentRL = 0;

typedef struct struct_message {
    int msg_vr;
    int msg_rl;
    bool hupe;
} struct_message;

struct_message myData;

bool macEqual(const uint8_t *a, const uint8_t *b) {
  for (int i = 0; i < 6; i++) if (a[i] != b[i]) return false;
  return true;
}

// Expo-Kurve: output = expo * x^3 + (1 - expo) * x  (normalisiert auf maxVal)
int applyExpo(int input, int maxVal, float expo) {
  float norm   = (float)input / (float)maxVal;
  float curved = expo * norm * norm * norm + (1.0f - expo) * norm;
  return (int)(curved * (float)maxVal);
}

// Einen Schritt von current Richtung target, mit unterschiedlichen Raten für Accel/Decel
int rampToward(int current, int target, int accelStep, int decelStep) {
  int diff = target - current;
  if (diff == 0) return current;

  // Weg von 0 = beschleunigen, Richtung 0 = verzögern
  bool accelerating = (diff > 0 && current >= 0) || (diff < 0 && current <= 0);
  int step = accelerating ? accelStep : decelStep;

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
  const uint8_t *mac = recv_info->src_addr;
  unsigned long now = millis();

  // Sender-Lock: kein aktiver Sender oder Timeout -> neuen Sender annehmen
  if (!hasSender || (now - lastPacketMillis > SENDER_TIMEOUT)) {
    memcpy(activeSenderMAC, mac, 6);
    hasSender = true;
  }

  // Pakete von anderen Sendern ignorieren
  if (!macEqual(mac, activeSenderMAC)) return;

  memcpy(&myData, incomingData, sizeof(myData));
  lastPacketMillis = now;

  // Expo anwenden -> wird Zielwert für den Ramp
  targetVR = applyExpo(myData.msg_vr, 2047, EXPO_VR);
  targetRL = applyExpo(myData.msg_rl, 1300, EXPO_RL);

  digitalWrite(Hupe, myData.hupe ? HIGH : LOW);
}

void setup() {
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);
  delay(1000);
  Serial.println("MD:startup");
  Serial.println("MT:startup");

  if (esp_now_init() != ESP_OK) return;
  esp_now_register_recv_cb(OnDataRecv);

  pinMode(Hupe, OUTPUT);
  delay(200);
}

void loop() {
  unsigned long now = millis();
  bool signalLost = (now - lastPacketMillis >= SIGNAL_TIMEOUT);

  if (signalLost) {
    digitalWrite(Hupe, LOW);
    targetVR = 0;
    targetRL = 0;
  }

  if (now - lastRampMillis >= RAMP_INTERVAL) {
    lastRampMillis = now;

    if (signalLost) {
      currentVR = rampToZero(currentVR, SIGNAL_LOSS_DECEL_STEP);
      currentRL = 0;
    } else {
      // Notbremsung: Joystick >= EMERGENCY_THRESHOLD in Gegenrichtung -> sofort auf 0
      // VR: Ramp + Notbremsung
      bool emBrakeVR = (currentVR != 0)
                    && (abs(targetVR) >= (int)(EMERGENCY_THRESHOLD * 2047))
                    && ((targetVR >= 0) != (currentVR >= 0));

      currentVR = emBrakeVR ? 0 : rampToward(currentVR, targetVR, ACCEL_STEP, DECEL_STEP);

      // RL: direkte Übertragung (Lenken braucht sofortige Reaktion)
      currentRL = targetRL;
    }

    Serial.print("MD:");
    Serial.println(currentVR);
    Serial.print("MT:");
    Serial.println(currentRL);
  }
}
