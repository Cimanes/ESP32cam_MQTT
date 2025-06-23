     
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