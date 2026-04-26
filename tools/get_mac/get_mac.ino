#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  uint8_t mac[6];
  WiFi.macAddress(mac);

  Serial.println("MAC-Adresse fuer broadcastAddress[]:");
  Serial.print("{0x");
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 0x10) Serial.print("0");
    Serial.print(mac[i], HEX);
    if (i < 5) Serial.print(", 0x");
  }
  Serial.println("}");
}

void loop() {}
