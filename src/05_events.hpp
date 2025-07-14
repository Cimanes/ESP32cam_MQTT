//======================================================
// VARIABLES
//======================================================
const size_t PRINT_LEN = 16;      // Size limit for mqtt received "payload" (used to limit serial.print)

struct Handler {                  // Handler structure to manage handlers
  const char* topic;    
  void (*handler)(const char* topic, const char* payload);
};
const Handler handlers[] = {      // topic  
  { "debug", handleDebug },       // cam/debug   **
  { "espIP", handleIP},           // cam/espIP
  { "reboot", handleReboot },     // cam/reboot
  { "flash", handleFlash },       // cam/flash   **
  { "photo", handlePhoto },       // cam/photo
  { "stream", handleStream },     // cam/stream  **
  { "period", handlePeriod },     // cam/period  **
  { "chunk", handleChunk },       // cam/chunk   **
  #ifdef WIFI_MANAGER
    { "wifi", handleWifi },       // cam/wifi
  #endif
  #ifdef USE_OTA
    { "OTA", handleOTA },         // cam/ota  
  #endif
  { "qty", handleQty },           // cfg/qty: Quality
  { "size", handleSize },         // cfg/size: Frame size
  { "bright", handleBright },     // cfg/bright: Brightness
  { "contrast", handleContrast }, // cfg/contrast 
  { "satur", handleSaturate },    // cfg/satur 
  { "mirror", handleMirror },    // cfg/mirror: Horizontal mirror
  { "flip", handleFlip },        // cfg/vflip: Vertical flip
  { "ceiling", handleCeiling },   // cfg/ceiling: Auto gain ceiling
  { "effect", handleEffect },     // cfg/effect: special effects
  { "awb", handleAwb },           // cfg/awb: Auto white balance
  { "wbg", handleWbgain },        // cfg/wbg: White balance gain
  { "wbmode", handleWbmode },     // cfg/wbmode: White balance mode
  { "lenc", handleLenc },         // cfg/lenc: Lens correction
  { "aec", handleAec },           // cfg/aec: Automatic Exposure Control
  { "expos", handleExpos },       // cfg/expos: Manual Exposure time (with AEC = OFF)
  { "agc", handleAgc },           // cfg/agc: Automatic Gain Control
  { "gain", handleGain },         // cfg/agcgain: Manual gain (with AGC = OFF)
  { "status", handleStatus },     // cfg/status: JSON with complete sensor status
};
const byte handlerCount = sizeof(handlers) / sizeof(handlers[0]);

//======================================
// MQTT Events
//======================================
void onmqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.printf_P(PSTR("MQTT Disconnected: %i \n"), (int)reason);
  if (WiFi.isConnected()) {
    mqttReconnectTimerID = timer.setTimeout(mqttReconnectTimer, []() { mqttClient.connect();});
    if (Debug) Serial.println(F("MQTT re-connecting..."));
  }
}

// When MQTT Connects:
// Publish curent ESP32-CAM feedback messages, 
// Then subscribe to commands
void onMqttConnect(bool sessionPresent) {
  handleStatus("", "");
  mqttClient.publish("fbCam/status", 1, false, payloadOut, strlen(payloadOut));
  timer.setTimeout(1000, mqttSubscribe);
}

void onmqttSubscribe(uint16_t packetId, uint8_t qos) {
  if(Debug) Serial.printf_P(PSTR("Sub. #%u \n"), packetId);
}

void onmqttUnsubscribe(uint16_t packetId) {
  if(Debug) Serial.printf_P(PSTR("Unsub. #%u \n"), packetId);
}

void onmqttPublish(uint16_t packetId) {
  if(Debug) Serial.printf_P(PSTR("Pub. #%u \n"), packetId);
}

void onmqttMessage(const char* topic, char* payload, const AsyncMqttClientMessageProperties properties, const size_t len, const size_t index, const size_t total) {
  if (Debug) {
    memcpy(payload + min(len, PRINT_LEN), "\0", 1);  // Ensure it's a valid C-string; limit printed length
    Serial.printf_P(PSTR(">[MQTT] %s: %s\n"), topic, payload);
    // Serial.printf_P(PSTR("qos: %i, dup: %i, retain: %i, len: %i, index: %i, total: %i \n"), properties.qos, properties.dup, properties.retain, len, index, total);
  }
  // Validate payload and sensor if applicable 
  if (!payload) {
    if (Debug) Serial.println(F("Null Payload!"));
    return;
  }
  if (!sensor && strncmp(topic, "cfg/", 4) == 0) { // fbCam/...
    if (Debug) Serial.println(F("Null Sensor!"));
    return;
  }
  // Process MQTT messages according to handlers  
  for (byte i = 0; i < handlerCount; i++) {
    if (strstr(topic, handlers[i].topic)) {
      handlers[i].handler(topic, payload);
      if (handlers[i].topic != "reboot" && handlers[i].topic != "photo") {
        sprintf(topicOut, "fbCam/%s", handlers[i].topic);
        mqttClient.publish(topicOut, 1, false, payloadOut);
        if (Debug) Serial.printf_P(PSTR("[MQTT]> %s: %s\n"), handlers[i].topic, payloadOut); 
      }
      return;
    }
  }
}

void initMqtt() {         
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onmqttDisconnect);
  mqttClient.onSubscribe(onmqttSubscribe);
  mqttClient.onUnsubscribe(onmqttUnsubscribe);
  mqttClient.onMessage(onmqttMessage);
  mqttClient.onPublish(onmqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCredentials(BROKER_USER, BROKER_PASS);
  mqttClient.setKeepAlive(MQTT_PING_INTERVAL);
  mqttClient.setClientId("esp32cam-async");  
  if(Debug) Serial.println(F("init MQTT done."));
}


//======================================
// Wifi Events
//======================================
void wifiEvents(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      if (Debug) { 
        Serial.println(WiFi.localIP());
        Serial.println(F("-> initWiFi done"));
      } 
      delay(3000);
      initMqtt();
      if (Debug) Serial.println(F("MQTT Connecting..."));
      mqttClient.connect();
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      if (Debug)   Serial.println(F("Wifi disconnected."));
      timer.deleteTimer(mqttReconnectTimerID);  // avoid reconnect to MQTT while reconnecting to Wi-Fi
      timer.setTimeout(wifiReconnectTimer, connectToWifi);    // attempt to reconnect to WiFi
      break;
  }
}

void serverEvents() {
  server.on("/snapshot", HTTP_GET, [](AsyncWebServerRequest *request){
    if (Debug) Serial.println(F("Snapshot requested"));
    flushCam();
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      request->send(500, "text/plain", "Camera capture failed");
      return;
    }

    AsyncResponseStream *response = request->beginResponseStream("image/jpeg", fb->len);
    response->addHeader("Content-Disposition", "inline; filename=snapshot.jpg");
    response->write(fb->buf, fb->len);
    request->send(response);

    esp_camera_fb_return(fb);
  }); 
}