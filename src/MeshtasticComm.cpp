#include "MeshtasticComm.h"
#include "ConfigManager.h"
#include "LD2450Manager.h"
#include <Arduino.h>

// Global serial interface for Meshtastic (UART1)
HardwareSerial* MeshtasticSerial = nullptr;

// Message buffer
std::string receivedChars;
unsigned long lastCharTime = 0;
const unsigned long CHAR_TIMEOUT = 100;  // 100ms timeout

void initMeshtasticComm() {
  MeshtasticSerial = &Serial1;  // Use UART1 (already initialized in main.cpp)
  Serial.println("Initializing Meshtastic UART communication...");
  Serial.println("Meshtastic UART initialized");
}

void sendPayloadViaMeshtastic(const String& payload) {
  Serial.println("UART-DEBUG: Sending payload via Meshtastic:");
  Serial.println(payload);
  Serial.printf("Payload size: %d bytes\n", payload.length());
  
  MeshtasticSerial->println(payload);
}

void checkForMeshtasticCommands() {
  // Check for incoming data on UART1
  while (MeshtasticSerial->available()) {
    char c = MeshtasticSerial->read();
    
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
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (error) {
      Serial.println("UART-DEBUG: JSON Parse Error: " + String(error.c_str()));
      return;
    }
    
    // Check magic word - try LD2450 config first
    if (doc.containsKey("m")) {
      String magicWord = doc["m"].as<String>();
      
      // Check if this is for LD2450
      if (magicWord == ld2450Manager.getConfig().magicWord.c_str()) {
        Serial.println("UART-DEBUG: LD2450 command detected");
        
        if (ld2450Manager.processConfigCommand(jsonStr)) {
          String ack = "{\"ack\":\"ok\",\"target\":\"LD2450\"}";
          MeshtasticSerial->println(ack);
          Serial.println("UART-DEBUG: LD2450 ACK sent: " + ack);
        } else {
          String ack = "{\"ack\":\"fail\",\"target\":\"LD2450\"}";
          MeshtasticSerial->println(ack);
          Serial.println("UART-DEBUG: LD2450 FAIL sent: " + ack);
        }
        return;
      }
    }
    
    // If not LD2450, ignore (could be for other devices)
    Serial.println("UART-DEBUG: Message does not match LD2450 magic word - ignoring");
    
  } else {
    Serial.println("UART-DEBUG: No valid JSON found in message");
  }
}