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
static char topicOut[20];
static char payloadOut[20];

unsigned int mqttReconnectTimerID      ;    // Timer to reconnect to MQTT after failed
const uint16_t mqttReconnectTimer = 15000;  // Delay to reconnect to Wifi after failed
int8_t payloadInt;                          // Dummy MQTT payload as integer

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

void handleDebug(const char* topic, const char* payload) { 
  Debug = payload[0] == '1' ? true : false;
  strncpy(payloadOut, Debug ? "1" : "0", 2);
}

void handleIP(const char* topic, const char* payload) {
  strncpy(payloadOut, esp_ip, strlen(esp_ip) + 1);
}

void handleReboot(const char* topic, const char* payload) {
  if (Debug) Serial.println(F("[MQTT]> Rebooting"));
  mqttClient.disconnect();
  timer.setTimeout(3000, []() { esp_restart(); } );
}

void handleFlash(const char* topic, const char* payload) {
  digitalWrite(FLASH_PIN, payload[0] == '1' ? HIGH : LOW);
  strncpy(payloadOut, digitalRead(FLASH_PIN) ? "1" : "0", 2);
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
  allowStream = payload[0] == '1' ? true : false;
  if (streamCheckTimerID >= 0) timer.deleteTimer(streamCheckTimerID);
  if (!allowStream)  stopStream();
  else streamCheckTimerID = timer.setInterval(streamCheckTimer, checkStream);
  strncpy(payloadOut, allowStream ? "1" : "0", 2);
}

void handlePeriod(const char* topic, const char* payload) { 
  streamTimer = atoi(payload);
  snprintf(payloadOut, 6, "%u", streamTimer);
}

void handleChunk(const char* topic, const char* payload) { 
  CHUNK_SIZE = atoi(payload);
  snprintf(payloadOut, 6, "%u", CHUNK_SIZE);
}

void handleQty(const char* topic, const char* payload) {
  payloadInt = atoi(payload);
  if (sensor->set_quality(sensor, payloadInt) != 0) {
    if (Debug) Serial.println(F("Quality set failed"));
    return;
  }
  itoa(sensor->status.quality, payloadOut, 10);
}

void handleSize(const char* topic, const char* payload) {
  payloadInt = atoi(payload);
  if (payloadInt <= 13) {  // validate index between 0 and 13
    if (sensor->set_framesize(sensor, (framesize_t)payloadInt) != 0) {
      if (Debug) Serial.println(F("Failed to set frame size"));
      return;
    }
    itoa(sensor->status.framesize, payloadOut, 10);  
  } 
  else {
    if(Debug) Serial.printf(PSTR("Invalid frame size: %d\n"), payloadInt);
  }
}

void handleBright(const char* topic, const char* payload) {
  payloadInt = atoi(payload);
  if (sensor->set_brightness(sensor, payloadInt) != 0) {
    if (Debug) Serial.println(F("Brightness set failed"));
    return;
  }
  itoa(sensor->status.brightness, payloadOut, 10);
}

void handleContrast(const char* topic, const char* payload) {
  payloadInt = atoi(payload);
  if (sensor->set_contrast(sensor, payloadInt) != 0) {
    if (Debug) Serial.println(F("Contrast set failed"));
    return;
  }
  itoa(sensor->status.contrast, payloadOut, 10);
}

void handleSaturate(const char* topic, const char* payload) {
  payloadInt = atoi(payload);
  if (sensor->set_saturation(sensor, payloadInt) != 0) {
    if (Debug) Serial.println(F("Saturate set failed"));
    return;
  }
  itoa(sensor->status.saturation, payloadOut, 10);
}

// Horizontal Mirror image
void handleMirror(const char *topic, const char* payload) {
  payloadInt = atoi(payload);
  if (sensor->set_hmirror(sensor, payloadInt) != 0) {
    if(Debug) Serial.println(F("Mirror set failed"));
    return;
  };
  itoa(sensor->status.hmirror, payloadOut, 10);
}

// Vertical Flip image
void handleFlip(const char *topic, const char* payload) {
  payloadInt = atoi(payload);
  if (sensor->set_vflip(sensor, payloadInt) != 0) {
    if(Debug) Serial.println(F("Flip set failed"));
    return;
  };
  itoa(sensor->status.vflip, payloadOut, 10);
}

// Gain ceiling
void handleCeiling(const char *topic, const char* payload) {
  payloadInt = atoi(payload);
  if (sensor->set_gainceiling(sensor, (gainceiling_t)payloadInt) != 0) {
    if(Debug) Serial.println(F("Flip set failed"));
    return;
  };
  itoa(sensor->status.gainceiling, payloadOut, 10);
}

// Special effects
void handleEffect(const char* topic, const char* payload) {
  payloadInt = atoi(payload);
  if (sensor->set_special_effect(sensor, payloadInt) != 0) {
    if (Debug) Serial.println(F("Saturate set failed"));
    return;
  }
  itoa(sensor->status.special_effect, payloadOut, 10);
}

// White balance mode selector
void handleWbmode(const char* topic, const char* payload) {
  payloadInt = atoi(payload);
  if (sensor->set_wb_mode(sensor, payloadInt) != 0) {
    if (Debug) Serial.println(F("Saturate set failed"));
    return;
  }
  itoa(sensor->status.wb_mode, payloadOut, 10);
}

// Automatic white balance
void handleAwb(const char *topic, const char* payload) {
  payloadInt = atoi(payload);
  if (sensor->set_whitebal(sensor, payloadInt) != 0) {
    if(Debug) Serial.println(F("AWB set failed"));
    return;
  };
  itoa(sensor->status.awb, payloadOut, 10);
}

// White balance gain
void handleWbgain(const char *topic, const char* payload) {
  payloadInt = atoi(payload);
  if (sensor->set_awb_gain(sensor, payloadInt) != 0) {
    if(Debug) Serial.println(F("WBgain set failed"));
    return;
  };

  itoa(sensor->status.awb_gain, payloadOut, 10);
}

// Lens correction
void handleLenc(const char *topic, const char* payload) {
  payloadInt = atoi(payload);
  if (sensor->set_lenc(sensor, payloadInt) != 0) {
    if(Debug) Serial.println(F("LenC set failed"));
    return;
  };
  itoa(sensor->status.lenc, payloadOut, 10);
}

// Automatic exposure control
void handleAec(const char *topic, const char* payload) {
  payloadInt = atoi(payload);
  if (sensor->set_exposure_ctrl(sensor, payloadInt) != 0) {
    if(Debug) Serial.println(F("AEC set failed"));
    return;
  };
  itoa(sensor->status.aec, payloadOut, 10);
}

// Manual Exposure time (with AEC = OFF)
void handleExpos(const char *topic, const char* payload) {
  payloadInt = atoi(payload);
  if (sensor->set_aec_value(sensor, 10 * payloadInt) != 0) {
    if(Debug) Serial.println(F("Expos. set failed"));
    return;
  };
  itoa(sensor->status.aec_value, payloadOut, 10);
}

// Automatic gain control
void handleAgc(const char *topic, const char* payload) {
  payloadInt = atoi(payload);
  if (sensor->set_gain_ctrl(sensor, payloadInt) != 0) {
    if(Debug) Serial.println(F("AGC set failed"));
    return;
  };
  itoa(sensor->status.agc, payloadOut, 10);
}

// Manual gain (with AGC = OFF)
void handleGain(const char *topic, const char* payload) {
  payloadInt = atoi(payload);
  if (sensor->set_agc_gain(sensor, payloadInt) != 0) {
    if(Debug) Serial.println(F("AGC set failed"));
    return;
  };
  itoa(sensor->status.agc_gain, payloadOut, 10);
}