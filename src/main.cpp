//======================================
// LIBRARIES
//======================================
#include <Arduino.h>
#include "00_globals.hpp"
#include "01_fileSys.hpp"
#include "02_wifi.hpp"
#include "03_esp32cam.hpp"
#include "04_mqtt.hpp"
#include "05_events.hpp"

//======================================
// SETUP
//======================================
void setup() {
  Serial.begin(115200);
  Serial.println();
  setup_camera();
  // initFS();
  WiFi.onEvent(wifiEvents);
  connectToWifi();      // includes initMqtt();
  serverEvents();

  initGPIO();
  streamCheckTimerID = timer.setInterval(streamCheckTimer, checkStream);
}

void loop() {
  // Accept client only if allowed and no one is streaming

  timer.run();
}
