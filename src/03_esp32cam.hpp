//======================================
// LIBRARIES
//======================================
#include "esp_camera.h"
#define CAMERA_MODEL_AI_THINKER   // Select camera model.
#include "camera_pins.h"
#include "mbedtls/base64.h"       // Required for base64 encoding.

//======================================
// ESP32CAM VARIABLES
//======================================
sensor_t *sensor = NULL;
bool allowStream = false;   // Controlled by MQTT
bool streaming = false;     // Indicates active client
int streamTimerID;
int streamCheckTimerID;
uint16_t streamTimer = 1000; // Delay between stream samples (ms)
const uint16_t streamCheckTimer = 3000; // Delay between stream checks

//======================================
// ESP32CAM FUNCTIONS 
//======================================
// Flush staled frames
void flushCam() {
  for (int i = 0; i < 2; i++) {
    camera_fb_t *fbt = esp_camera_fb_get();
    if (fbt) esp_camera_fb_return(fbt);
    delay(30);  // Give time for new frame
  }
  if(Debug) Serial.println(F("Flushed"));
}

void stopStream() {
  if (client.connected()) {
    client.stop();
    if (Debug) Serial.println(F("Stream stopped"));
  }
  streaming = false;
  timer.deleteTimer(streamTimerID);
}

void sendFrame() {
  if (!allowStream || !streaming || !client.connected()) {
    stopStream();  // clean up
    return;
  }
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;
  client.printf_P(PSTR("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n"), fb->len);
  client.write(fb->buf, fb->len);
  client.write("\r\n", 2);
  esp_camera_fb_return(fb);
  if (Debug && streamTimer > 999) Serial.println(F("Frame sent"));
}

void startStream(WiFiClient newClient) {
  timer.deleteTimer(streamCheckTimerID);
  if (streamTimerID >= 0) timer.deleteTimer(streamTimerID);  // just in case
  client = newClient;
  if (client.connected()) {
    flushCam();
    streaming = true;
    const char *header =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
    client.write(header, strlen(header));
    if (Debug) Serial.println(F("Streaming"));
    streamTimerID = timer.setInterval(streamTimer, sendFrame);
  }
}

void checkStream() {
  if (!streaming && allowStream) {
    WiFiClient newClient = mjpegServer.available();
    if (newClient) startStream(newClient);
  }
  if (Debug) Serial.printf_P(PSTR("Stream check: %u\n"), (uint8_t)allowStream);
}

void setup_camera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_HVGA;
  config.jpeg_quality = 15;
  config.fb_count = 1;

  // if(config.pixel_format == PIXFORMAT_JPEG){
  //   if(psramFound()){
  //     config.jpeg_quality = 10;
  //     config.fb_count = 2;
  //     config.grab_mode = CAMERA_GRAB_LATEST;
  //   } 


  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf(PSTR("Camera init failed. Error: 0x%x"), err);
    while(true);
  }
  sensor = esp_camera_sensor_get();
}



unsigned char* codedPhoto(size_t &codedLen)  {
  flushCam() ; 

  // Capture frame
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    if (Debug) Serial.println(F("Camera capture failed"));
    return nullptr;
  }
  if (Debug) Serial.printf(PSTR("fb->len: %d\n"), fb->len);
  
  // Calculate required memroy for base64 encoding
  size_t bufferLen = 4 * (1 + (fb->len + 2) / 3);     // base64 expands data ~33%
  unsigned char *codedBuf = (unsigned char *)malloc(bufferLen);
  if (!codedBuf) {
    if (Debug) Serial.println(F("Not enough memory for Base64 encoding"));
    esp_camera_fb_return(fb);
    codedLen = 0;
    return nullptr;
  }

  // Determine the output size 
  size_t outputLen = 0;
  int ret = mbedtls_base64_encode(codedBuf, bufferLen, &codedLen, fb->buf, fb->len);
  if (ret != 0) {
    if (Debug) Serial.printf(PSTR("Base64 encode failed: %d\n"), ret);
    free(codedBuf);
    esp_camera_fb_return(fb);
    codedLen = 0;
    return nullptr;
  }

  // Null-terminate the codedBuf string for safety
  codedBuf[codedLen] = 0;
  if (Debug) Serial.printf(PSTR("Base64 size: %d\n"), codedLen);
  esp_camera_fb_return(fb);
  return codedBuf;
}