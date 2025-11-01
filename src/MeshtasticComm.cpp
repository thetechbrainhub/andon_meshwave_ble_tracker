#include "MeshtasticComm.h"
#include "Config.h"
#include "ConfigManager.h"
#include "BeaconTracker.h"
#include "JsonUtils.h"
#include <Arduino.h>

// Global serial interface for Meshtastic
HardwareSerial MeshtasticSerial(1);  // UART1

// Message buffer
std::string receivedChars;
unsigned long lastCharTime = 0;
const unsigned long CHAR_TIMEOUT = 100;  // 100ms timeout

void initMeshtasticComm() {
  Serial.println("Initializing Meshtastic UART communication...");
  // Note: .begin() is called in main.cpp setup() before this function
  Serial.println("Meshtastic UART initialized");
}

void sendBeaconToMeshtastic(const std::string& address, DeviceInfo& device, float lastSeenOverride) {
  String json = generateBeaconJSON(address, device, lastSeenOverride);
  
  Serial.println("UART-DEBUG: Sende Beacon-Daten an Meshtastic:");
  Serial.println("----------------------------------------");
  Serial.println(json);
  Serial.println("----------------------------------------");
  Serial.println("UART-DEBUG: Daten gesendet!");
  
  MeshtasticSerial.println(json);
}

void checkForMeshtasticCommands() {
  // Check for incoming data
  while (MeshtasticSerial.available()) {
    char c = MeshtasticSerial.read();
    
    // Debug output for each character
    if (c >= 32 && c <= 126) {
      Serial.printf("UART-DEBUG: [CHAR] 0x%02X ('%c') - Buffer size: %d\n", c, c, (int)receivedChars.length() + 1);
    } else {
      Serial.printf("UART-DEBUG: [CHAR] 0x%02X (non-printable) - Buffer size: %d\n", c, (int)receivedChars.length() + 1);
    }
    
    receivedChars += c;
    lastCharTime = millis();
    
    // Process on newline or closing brace
    if (c == '\n' || c == '}') {
      Serial.println("UART-DEBUG: *** TRIGGER: Received '" + String(c) + "' - processing buffer ***");
      processReceivedJSON();
      receivedChars.clear();
    }
  }
  
  // Timeout check
  if (!receivedChars.empty() && (millis() - lastCharTime > CHAR_TIMEOUT)) {
    Serial.println("UART-DEBUG: Buffer timeout - clearing");
    receivedChars.clear();
  }
  
  // Idle message
  if (receivedChars.empty() && !MeshtasticSerial.available()) {
    static unsigned long lastIdleTime = 0;
    if (millis() - lastIdleTime > 10000) {  // Every 10 seconds
      Serial.println("UART-DEBUG: checkForMeshtasticCommands() running - waiting for data...");
      lastIdleTime = millis();
    }
  }
}

void processReceivedJSON() {
  if (receivedChars.empty()) {
    return;
  }
  
  Serial.println("========================================");
  Serial.println("UART-DEBUG: Processing message:");
  Serial.printf("UART-DEBUG: Buffer length: %d bytes\n", (int)receivedChars.length());
  Serial.println(receivedChars.c_str());
  Serial.println("========================================");
  
  // Find JSON
  size_t jsonStart = receivedChars.find('{');
  size_t jsonEnd = receivedChars.find('}');
  
  if (jsonStart != std::string::npos && jsonEnd != std::string::npos && jsonEnd > jsonStart) {
    Serial.printf("UART-DEBUG: Found '{' at position %d\n", (int)jsonStart);
    Serial.printf("UART-DEBUG: Found '}' at position %d\n", (int)jsonEnd);
    
    String jsonStr = receivedChars.substr(jsonStart, jsonEnd - jsonStart + 1).c_str();
    Serial.println("UART-DEBUG: Extracted JSON: " + jsonStr);
    
    // Parse JSON
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (!error && doc.containsKey("target")) {
      String targetGateway = doc["target"].as<String>();
      
      // ✅ FIX: Use .c_str() to compare std::string with String
      if (targetGateway == GATEWAY_ID.c_str()) {
        Serial.println("UART-DEBUG: Message for this gateway (" + String(GATEWAY_ID.c_str()) + ") - processing...");
        
        // Process config command
        if (ConfigManager::processConfigCommand(jsonStr)) {
          // Success
          String ack = "{\"ack\":\"" + String(GATEWAY_ID.c_str()) + "\",\"ok\":true}";
          MeshtasticSerial.println(ack);
          Serial.println("UART-DEBUG: Acknowledgment sent: " + ack);
          
        } else {
          Serial.println("UART-DEBUG: Configuration update failed!");
          
          // Send error acknowledgment
          String error = "{\"ack\":\"" + String(GATEWAY_ID.c_str()) + "\",\"ok\":false}";
          MeshtasticSerial.println(error);
          Serial.println("UART-DEBUG: Error response sent: " + error);
        }
      } else {
        Serial.println("UART-DEBUG: Message not for this gateway (" + String(GATEWAY_ID.c_str()) + ") - target: " + targetGateway);
      }
    } else {
      Serial.println("UART-DEBUG: JSON parsing failed or no target field found - ignoring");
    }
  } else {
    Serial.println("UART-DEBUG: No '{' found in message");
  }
}