//======================================
// GLOBAL LIBRARIES
//======================================
#include <Arduino.h>
#include <SimpleTimer.h>


//======================================
// GLOBAL VARIABLES
//======================================
#define FLASH_PIN 4
bool Debug = true;
SimpleTimer timer;          // SimpleTimer object
static char strBuffer[100];      // Dummy char array to send MQTT message

// =====================================
// Setup GPIO's
// ======================================
// struct to assign GPIO pins for each topic
struct pinMap { const char* topic; const byte gpio; const bool value; }; 
const pinMap gpioPins[] = {       
  { "flash", FLASH_PIN, LOW }
};
const byte gpioCount = sizeof(gpioPins) / sizeof(gpioPins[0]);


void initGPIO() {
  for (byte i = 0; i < gpioCount; i++) {
    pinMode(gpioPins[i].gpio, OUTPUT);
    digitalWrite(gpioPins[i].gpio, gpioPins[i].value);
  }
}