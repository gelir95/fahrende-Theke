/*
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at https://RandomNerdTutorials.com/esp-now-esp32-arduino-ide/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/
#include <esp_now.h>
#include <WiFi.h>

// REPLACE WITH YOUR RECEIVER MAC Address
uint8_t broadcastAddress[] = {0x78, 0x21, 0x84, 0xA0, 0x27, 0x94};

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
  int msg_vr;
  int msg_rl;
  bool hupe;
} struct_message;

// Create a struct_message called myData
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

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
 
void setup() {
  // Init Serial Monitor
  Serial.begin(115200);

  pinMode(33, INPUT_PULLUP);
 
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(esp_now_send_cb_t(OnDataSent));
  
  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
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

  if (vr_input > 2100){
    vr_temp = map(vr_input, 2100, 4096, 0, -2047);
  }
  else if (vr_input < 1900){
    vr_temp = map(vr_input, 1900, 0, 0, 2047);

  }
  else{
    vr_temp = 0;
  }

  if (rl_input > 2100){
    rl_temp = map(rl_input, 2100, 4096, 0, -1300);
  }
  else if (rl_input < 1900){
    rl_temp = map(rl_input, 1900, 0, 0, 1300);

  }
  else{
    rl_temp = 0;
  }

  poti_temp = map(poti,0,3100,0,100);

  if (poti_temp>100){
    poti_temp = 100;
  }
  
  rl_temp = (rl_temp * poti_temp)/100;
  vr_temp = (vr_temp * poti_temp)/100;


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
      
    // Send message via ESP-NOW
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
     
    if (result == ESP_OK) {
      //Serial.println("Sent with success");
    }
    else {
      //Serial.println("Error sending the data");
    }
  }
}
