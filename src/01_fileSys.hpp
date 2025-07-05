// =============================================
// LIBRARIES
// =============================================
#include "LittleFS.h"   // OPTIONAL: available for SPIFFS in ESP32 only

// =============================================
// VARIABLES
// =============================================
const byte paramSize = 25;  // Maximum size for SSID and Password
// uint16_t totalKB = 0;
// uint16_t usedKB  = 0;

// =============================================
// MANAGE FILE SYSTEM
// =============================================
void initFS() {
  if (!LittleFS.begin()) {
    if (Debug) Serial.println(F("File Sys mount - FAIL"));
  }
  else {
    if (Debug) Serial.println(F("File Sys mounted"));
    // totalKB = LittleFS.totalBytes()/1024;  // Total memory in LittleFS
    // usedKB = LittleFS.usedBytes()/1024;    // Used memory in LittleFS
  }
}


// ===============================================================================
// Write file to LittleFS
// ===============================================================================
void writeFile(fs::FS &fs, const char * path, const char * message){
  if (Debug) Serial.printf_P(PSTR("Writing file: %s\r\n"), path);
  File file = fs.open(path, "w");
  if (!file) {
    if (Debug) Serial.println(F("Open file to write - FAIL"));
    return;
  }
  if (!file.print(message))  {
    if (Debug) Serial.println(F("Write - FAIL"));
  }
  file.close();    // Not required in Arduino enviroment
}

// ===============================================================================
// Read file from LittleFS into char* variable
// ===============================================================================
void fileToCharPtr(fs::FS &fs, const char* path, char* buffer) {
  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) {
    if (Debug) Serial.println(F("no file"));
    buffer[0] = '\0'; // Ensure the buffer is null-terminated
    return;
  }
  if (Debug) Serial.println(F("File"));
  byte i = 0;  // Buffer index (needs to be as big as paramSize (25) -> byte is enough)
  while (file.available() && i < paramSize - 1) {
    buffer[i++] = (char)file.read();
  }
  buffer[i] = '\0';
  file.close();    // Not required in Arduino enviroment
}

// ===============================================================================
// Delete File from LittleFS
// ===============================================================================
void deleteFile(fs::FS &fs, const char * path){
  if (Debug) Serial.printf_P(PSTR("Deleting: %s"), path);
  if(fs.remove(path)) { if (Debug)  Serial.println(F("- file deleted")); }
  else { if (Debug)  Serial.println(F("- delete failed")); }  
}

// ===============================================================================
// Read file from LittleFS into const char*
// ===============================================================================
// const char* readFile(fs::FS &fs, const char * path) {
//   if (Debug) Serial.printf_P(PSTR("Reading file: %s\r\n"), path);
//   File file = fs.open(path, "r");
//   if (Debug) 
//     if(!file || file.isDirectory()){
//         Serial.println(F("- file not found"));
//       return nullptr;
//     }
//     // Allocate memory for the file content + null terminator
//     char* fileContent = new char[file.size() + 1]; 
//     // Read the file content: 
//     if (file.readBytes(fileContent, file.size()) != file.size()) {
//         Serial.println(F("- error reading file"));
//       delete[] fileContent; // Free memory
//       file.close();
//       return nullptr;
//     }
//   fileContent[file.size()] = '\0';  // Null-terminate the string
//   file.close();
//   return fileContent;
// }



// ===============================================================================
// Get size from data-file in LittleFS
// ===============================================================================
// const unsigned int getFileSize(fs::FS &fs, const char * path){
//   File file = fs.open(path, "r");
//   if(!file){
//     if (Debug)  Serial.println(F("Open file to check size - FAIL"));
//     return 0;
//   }
//   return file.size();
// }

// ===============================================================================
// Append data to file in LittleFS
// ===============================================================================
// void appendToFile(fs::FS &fs, const char * path, const char * message) {
//   if (Debug)   { Serial.print(F("Appending to: ")); Serial.println(path); }
  
//   File file = fs.open(path, "a");
//   if (!file) {
//     if (Debug)  Serial.println(F("Open file to append - FAIL"));
//     return;
//   }
//   if (file.print(message) && file.print(",")) {
//     if (Debug)  Serial.println(F("- msgOut. appended"));
//   }
//   else { 
//     if (Debug)  Serial.println(F("Append - FAIL")); 
//   }
//   file.close();
// }