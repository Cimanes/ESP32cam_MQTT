//======================================
// LIBRARIES
//======================================
#include <Arduino.h>
#include "00_globals.hpp"
#include "01_fileSys.hpp"
#include "02_esp32cam.hpp"
#include "03_wifi.hpp"
#include "04_mqtt.hpp"
#include "05_events.hpp"


//======================================
// SETUP
//======================================
void setup() {
  Serial.begin(115200);

  Serial.setDebugOutput(true);
  Serial.println();
  
  // initFS();
  WiFi.onEvent(wifiEvents);
  connectToWifi();    // includes initMqtt();
  pinMode(4, OUTPUT);
  setup_camera();
}

void loop() {
  timer.run();
}
