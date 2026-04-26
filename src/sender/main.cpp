#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// Fuer echte Hardware: Empfaenger-MAC eintragen
// Fuer Wokwi-Test: Broadcast (verbindet sich mit jedem ESP32 in Reichweite)
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct struct_message {
  int msg_vr;
  int msg_rl;
  bool hupe;
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

unsigned long previousMillis = 0;
int vr_input;
int rl_input;
int poti;
bool dead_input;
int vr_temp;
int rl_temp;
int poti_temp;

void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup() {
  Serial.begin(115200);
  pinMode(33, INPUT_PULLUP);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

void loop() {
  vr_input = analogRead(35);
  delay(10);
  rl_input = analogRead(34);
  delay(10);
  poti = map(analogRead(32), 0, 4095, 4095, 0);
  delay(10);
  dead_input = !digitalRead(33);
  myData.hupe = dead_input;

  if (vr_input > 2100)
    vr_temp = map(vr_input, 2100, 4096, 0, -2047);
  else if (vr_input < 1900)
    vr_temp = map(vr_input, 1900, 0, 0, 2047);
  else
    vr_temp = 0;

  if (rl_input > 2100)
    rl_temp = map(rl_input, 2100, 4096, 0, -1300);
  else if (rl_input < 1900)
    rl_temp = map(rl_input, 1900, 0, 0, 1300);
  else
    rl_temp = 0;

  poti_temp = map(poti, 0, 3100, 0, 100);
  if (poti_temp > 100) poti_temp = 100;

  rl_temp = (rl_temp * poti_temp) / 100;
  vr_temp = (vr_temp * poti_temp) / 100;

  myData.msg_vr = vr_temp;
  myData.msg_rl = rl_temp;

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 50) {
    previousMillis = currentMillis;

    Serial.print(vr_input);
    Serial.print("    ");
    Serial.print(rl_input);
    Serial.print("    ");
    Serial.print(poti);
    Serial.print("    ");
    Serial.println(dead_input);

    esp_now_send(broadcastAddress, (uint8_t *)&myData, sizeof(myData));
  }
}
