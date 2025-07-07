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
uint16_t CHUNK_SIZE = 1000;  // adjust as needed

AsyncMqttClient mqttClient;
const char* subTopics[] = { "cam/#", "cfg/#" };
const byte subLen = sizeof(subTopics) / sizeof(subTopics[0]);
// static char topicOut[20];  // "fb/" = 4 + null terminator
static char payloadOut[20];

unsigned int mqttReconnectTimerID      ; // Timer to reconnect to MQTT after failed
const uint16_t mqttReconnectTimer = 15000; // Delay to reconnect to Wifi after failed

//======================================
// MQTT Message handlers
//======================================

void mqttSubscribe() {
  if(Debug) Serial.println(F("Subscribing:"));
  for (byte i = 0; i < subLen; i++) {
    mqttClient.subscribe(subTopics[i], 2);
    if(Debug) Serial.println(subTopics[i]);
  }
}
void handleFlash(const char *topic, const char *payload) {
  digitalWrite(FLASH_PIN, payload[0] == '1');
  strncpy(payloadOut, digitalRead(FLASH_PIN) ? "1" : "0", 2);
  mqttClient.publish("fbCam/flash", 1, true, payloadOut);
  if (Debug) Serial.printf_P(PSTR("[MQTT]> flash: %s\n"), payloadOut);
}



void handleIP(const char* topic, const char* payload) {
  strncpy(payloadOut, esp_ip, sizeof(esp_ip));
  mqttClient.publish("fbCam/espIP", 1, false, esp_ip);
  if (Debug) Serial.printf_P(PSTR("[MQTT]> espIP: %s\n"), payloadOut);
}

void handleDebug(const char* topic, const char* payload) { 
  Debug = payload[0] == '1';
  strncpy(payloadOut, Debug ? "1" : "0", 2);
  mqttClient.publish("fbCam/debug", 1, false, payloadOut); 
  if (Debug) Serial.printf_P(PSTR("[MQTT]> Debug: %s\n"), payloadOut); 
}

void handleChunk(const char* topic, const char* payload) { 
  CHUNK_SIZE = atoi(payload);
  snprintf(payloadOut, 6, "%u", CHUNK_SIZE);
  mqttClient.publish("fbCam/chunk", 1, false, payloadOut); 
  if (Debug) Serial.printf_P(PSTR("[MQTT]> Debug: %s\n"), payloadOut); 
} 

void handleReboot(const char* topic, const char* payload) {
  if (Debug) Serial.println(F("[MQTT]> Rebooting"));

  mqttClient.disconnect();
  timer.setTimeout(3000, []() { esp_restart(); } );
}

void handleQty(const char* topic, const char* payload) {
  if (!sensor || !payload) {
    if (Debug) Serial.println(F("Sensor or payload is null"));
    return;
  }

  byte val = (byte)atoi(payload);
  if (val <= 0 || val > 63) {
    if (Debug) Serial.println(F("Invalid qty"));
    return;
  }

  if (sensor->set_quality(sensor, val) != 0) {
    if (Debug) Serial.println(F("Quality set failed"));
    return;
  }

  itoa(sensor->status.quality, payloadOut, 10);
  mqttClient.publish("fbCam/qty", 1, false, payloadOut);
  if (Debug) Serial.printf_P(PSTR("[MQTT]> qty: %s\n"), payloadOut);
}

void handleSize(const char* topic, const char* payload) {
  if (!sensor || !payload) {
    if (Debug) Serial.println(F("Sensor or payload is null"));
    return;
  }

  byte sizeIndex = (byte)atoi(payload);
  if (sizeIndex <= 13) {  // validate index between 0 and 13
    if (sensor->set_framesize(sensor, (framesize_t)sizeIndex) != 0) {
      if (Debug) Serial.println(F("Failed to set frame size"));
      return;
    }
    itoa(sensor->status.framesize, payloadOut, 10);  
    mqttClient.publish("fbCam/frsize", 1, false, payloadOut);
    if(Debug) Serial.printf(PSTR("[MQTT]> frame size: %s\n"), payloadOut);
  } 
  else {
    if(Debug) Serial.printf(PSTR("Invalid frame size index: %d\n"), sizeIndex);
  }
}

void handlePhoto(const char* topic, const char* payload) {
  size_t codedLen = 0;
  unsigned char *codedBuf = codedPhoto(codedLen);
  if (!codedBuf || codedLen == 0) {
    if (Debug) Serial.println(F("No codedBuf image to publish"));
    return;
  }
  
  // Chunk and publish parts
  for (size_t i = 0; i < codedLen; i += CHUNK_SIZE) {
    if (Debug) Serial.printf(PSTR("%d - "), i);
    size_t len = (i + CHUNK_SIZE < codedLen) ? CHUNK_SIZE : (codedLen - i);
    mqttClient.publish("fbCam/photo", 0, false, (const char *)(codedBuf + i), len);
  }
  mqttClient.publish("fbCam/photo", 0, false, "done");
  
  free(codedBuf);
  if (Debug) Serial.printf(PSTR("\n[MQTT]> photo\n"));
}

void handleStream(const char* topic, const char* payload) {
  allowStream = payload[0] == '1';
  if (streamCheckTimerID >= 0) timer.deleteTimer(streamCheckTimerID);
  if (!allowStream)  stopStream();
  else streamCheckTimerID = timer.setInterval(streamCheckTimer, checkStream);

  strncpy(payloadOut, allowStream ? "1" : "0", 2);
  mqttClient.publish("fbCam/stream", 1, false, payloadOut);
  if (Debug) Serial.printf_P(PSTR("[MQTT]> stream: %s\n"), payloadOut);   
}

void handlePeriod(const char* topic, const char* payload) { 
  streamTimer = atoi(payload);
  snprintf(payloadOut, 6, "%u", streamTimer);
  mqttClient.publish("fbCam/period", 1, false, payloadOut); 
  if (Debug) Serial.printf_P(PSTR("[MQTT]> Period: %s\n"), payloadOut);
 }

void handleBright(const char* topic, const char* payload) {
  if (!sensor || !payload) {
    if (Debug) Serial.println(F("Sensor or payload is null"));
    return;
  }

  const int8_t val = atoi(payload);
  if (val < -2 || val > 2) {
    if (Debug) Serial.println(F("Invalid brightness"));
    return;
  }
  if (sensor->set_brightness(sensor, val) != 0) {
    if (Debug) Serial.println(F("Brightness set failed"));
    return;
  }

  itoa(sensor->status.brightness, payloadOut, 10);
  mqttClient.publish("fbCam/bright", 1, false, payloadOut);
  if (Debug) Serial.printf_P(PSTR("[MQTT]> bright: %s\n"), payloadOut);
}

void handleContrast(const char* topic, const char* payload) {
  if (!sensor || !payload) {
    if (Debug) Serial.println(F("Sensor or payload is null"));
    return;
  }

  const int8_t val = atoi(payload);
  if (val < -2 || val > 2) {
    if (Debug) Serial.println(F("Invalid contrast"));
    return;
  }
  if (sensor->set_contrast(sensor, val) != 0) {
    if (Debug) Serial.println(F("Contrast set failed"));
    return;
  }

  itoa(sensor->status.contrast, payloadOut, 10);
  mqttClient.publish("fbCam/contrast", 1, false, payloadOut);
  if (Debug) Serial.printf_P(PSTR("[MQTT]> contrast: %s\n"), payloadOut);
}

void handleSaturate(const char* topic, const char* payload) {
  if (!sensor || !payload) {
    if (Debug) Serial.println(F("Sensor or payload is null"));
    return;
  }

  const int8_t val = atoi(payload);
  if (val < -2 || val > 2) {
    if (Debug) Serial.println(F("Invalid saturate"));
    return;
  }
  if (sensor->set_saturation(sensor, val) != 0) {
    if (Debug) Serial.println(F("Saturate set failed"));
    return;
  }

  itoa(sensor->status.saturation, payloadOut, 10);
  mqttClient.publish("fbCam/saturate", 1, false, payloadOut);
  if (Debug) Serial.printf_P(PSTR("[MQTT]> saturate: %s\n"), payloadOut);
}

void handleMirror(const char *topic, const char *payload) {
  const int8_t val = atoi(payload);
  if (val < 0 || val > 1) {
    if (Debug) Serial.println(F("Invalid saturate"));
    return;  if (sensor->set_hmirror(sensor, val) != 0) {
    if(Debug) Serial.println(F("Mirror set failed"));
    return;
  };

  itoa(sensor->status.hmirror, payloadOut, 10);
  mqttClient.publish("fbCam/hmirror", 1, false, payloadOut);
  if (Debug) Serial.printf_P(PSTR("[MQTT]> hmirror: %s\n"), payloadOut);
}

void handleFlip(const char *topic, const char *payload) {
  const int8_t val = atoi(payload);
  if (val < 0 || val > 1) {
    if (Debug) Serial.println(F("Invalid saturate"));
    return;
  }  if (sensor->set_vflip(sensor, val) != 0) {
    if(Debug) Serial.println(F("Flip set failed"));
    return;
  };

  itoa(sensor->status.vflip, payloadOut, 10);
  mqttClient.publish("fbCam/vflip", 1, false, payloadOut);
  if (Debug) Serial.printf_P(PSTR("[MQTT]> vflip: %s\n"), payloadOut);
}