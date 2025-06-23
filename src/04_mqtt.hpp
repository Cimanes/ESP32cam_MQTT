//======================================
// LIBRARIES
//======================================
#include <AsyncMqttClient.h>


//======================================
// GLOBAL VARIABLES
//======================================
#define MQTT_HOST IPAddress(192,168,1,128)  // broker IP
#define MQTT_PORT 1883
#define BROKER_USER "cimanes"     // MQTT BROKER USER
#define BROKER_PASS "Naya-1006"   // MQTT BROKER PASSWORD
#define MQTT_PING_INTERVAL 30     // MQTT "Keep-Alive" interval (s)

// Publish in chunks (MQTT payload max ~256k, adjust as needed)
const int chunkSize = 1000;
AsyncMqttClient mqttClient;
unsigned long mqttReconnectTimerID      ; // Timer to reconnect to MQTT after failed
const uint16_t mqttReconnectTimer = 15000; // Delay to reconnect to Wifi after failed


//======================================
// MQTT Functions
//======================================
void onmqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.printf_P(PSTR("MQTT Disconnected: %i \n"), (int)reason);
  if (WiFi.isConnected()) {
    Serial.println(F("MQTT re-connecting..."));
    mqttReconnectTimerID = timer.setTimeout(mqttReconnectTimer, []() { mqttClient.connect();});
    mqttClient.connect();
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.println(F("Subscribing:"));
  mqttClient.subscribe("esp32cam/cmd", 2);
}

void onmqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.printf_P(PSTR("Sub. #%u \n"), packetId);
}

void onmqttUnsubscribe(uint16_t packetId) {
  Serial.printf_P(PSTR("Unsub. #%u \n"), packetId);
}

void onmqttPublish(uint16_t packetId) {
  Serial.printf_P(PSTR("Pub. #%u \n"), packetId);
}

void onmqttMessage(const char* topic, char* payload, const AsyncMqttClientMessageProperties properties, const size_t len, const size_t index, const size_t total) {

  String message;
  for (unsigned int i=0; i<len; i++) {
    message += (char)payload[i];
  }
  Serial.print(F(">[MQTT]: "));
  Serial.println(message);

  if (message == "capture") {
    // Flush staled frames
    // for (int i = 0; i < 2; i++) {
    //   camera_fb_t *fbt = esp_camera_fb_get();
    //   if (fbt) esp_camera_fb_return(fbt);
    //   delay(30);  // Give time for new frame
    // }
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println(F("Camera capture failed"));
      return;
    }
    // Convert to base64
    String encoded = base64::encode(fb->buf, fb->len);
    Serial.println("Base64 length: " + String(encoded.length()));
    Serial.println(encoded.substring(0, 10));  // Optional: to verify start

    for (int i = 0; i < encoded.length(); i += chunkSize) {
      String chunk = encoded.substring(i, i + chunkSize);
      mqttClient.publish("esp32cam/photo", 0, false, chunk.c_str());
    }
    uint16_t packetId = mqttClient.publish("esp32cam/photo/done", 0, false, "done");

    Serial.printf(PSTR("Published photo, packet ID: %u\n"), packetId);
    esp_camera_fb_return(fb);
  }
  
  else if (message == "flash_on") {
    // Turn on flashlight (if hardware supports)
    digitalWrite(4, HIGH);
    mqttClient.publish("esp32cam/flash", 0, false,  "true");
  } 

  else if (message == "flash_off") {
    // Turn off flashlight
    digitalWrite(4, LOW);
    mqttClient.publish("esp32cam/flash", 0, false,  "false");
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
  // mqttClient.setCredentials(BROKER_USER, BROKER_PASS);
  mqttClient.setKeepAlive(MQTT_PING_INTERVAL);
  mqttClient.setClientId("esp32cam-async");  
  Serial.println(F("init MQTT done."));
}

