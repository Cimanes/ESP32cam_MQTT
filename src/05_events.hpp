//======================================================
// VARIABLES
//======================================================
const size_t PRINT_LEN = 16;      // Size limit for mqtt received "payload" (used to limit serial.print)

struct Handler {                  // Handler structure to manage handlers
  const char* topic;    
  void (*handler)(const char* topic, const char* payload);
};
const Handler handlers[] = {      // topic:
  { "photo", handlePhoto },       // cam/photo
  { "flash", handleFlash },       // cam/flash
  { "qty", handleQty },           // cam/qty
  { "size", handleSize },         // cam/size
  { "debug", handleDebug },       // esp/debug
  { "espIP", handleIP},           // cam/espIP
  { "reboot", handleReboot },     // cam/reboot
  #ifdef WIFI_MANAGER
    { "wifi", handleWifi },       // cam/wifi
  #endif
  #ifdef USE_OTA
    { "OTA", handleOTA }
  #endif
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

void onMqttConnect(bool sessionPresent) {
  if(Debug) Serial.println(F("Subscribing:"));
  for (byte i = 0; i < subLen; i++) {
    mqttClient.subscribe(subTopics[i], 2);
    if(Debug) Serial.println(subTopics[i]);
  }
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
    memcpy(payload + min(len, PRINT_LEN), "\0", 1);  // Ensure it's a valid C-string
    Serial.printf_P(PSTR(">[MQTT] %s: %s\n"), topic, payload);
    // Serial.printf_P(PSTR("qos: %i, dup: %i, retain: %i, len: %i, index: %i, total: %i \n"), properties.qos, properties.dup, properties.retain, len, index, total);
  }

  // Process MQTT messages according to handlers
  for (byte i = 0; i < handlerCount; i++) {
    if (strstr(topic, handlers[i].topic)) {
      handlers[i].handler(topic, payload);
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
  Serial.println(F("init MQTT done."));
}


//======================================
// Wifi Events
//======================================
void wifiEvents(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
      case SYSTEM_EVENT_STA_GOT_IP:
        if (Debug) { 
          Serial.println(WiFi.localIP());
          Serial.println(F(" -> initWiFi done"));
        } 
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