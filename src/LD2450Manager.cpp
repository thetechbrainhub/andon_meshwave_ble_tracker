#include "LD2450Manager.h"
#include <ArduinoJson.h>
#include <Preferences.h>

// Global instance
LD2450Manager ld2450Manager;

LD2450Manager::LD2450Manager() 
  : lastReadTime(0), sensorInitialized(false), closestDistanceCm(600), bufferPos(0) {
  loadDefaultConfig();
  
  // Initialize all targets
  for (int i = 0; i < 3; i++) {
    targets[i].state = ABSENT;
    targets[i].previousState = ABSENT;
    targets[i].stateChangeTime = 0;
    targets[i].lastX = 0;
    targets[i].lastY = 0;
    targets[i].lastSpeed = 0;
    targets[i].lastDistance = 0;
    targets[i].valid = false;
    targets[i].stateChanged = false;
  }
}

void LD2450Manager::loadDefaultConfig() {
  config.rangeMaxCm = 300;
  config.debounceMs = 2500;
  config.filterEnable = true;
  config.sensorEnable = true;
  config.deviceName = "LD2450_A";
  config.magicWord = "LD2450";
}

void LD2450Manager::init() {
  // Load config from NVS first (if exists)
  loadFromNVS();
  
  // Initialize UART2 for LD2450 (256000 baud)
  // RX = GPIO8, TX = GPIO9
  Serial2.begin(256000, SERIAL_8N1, 8, 9);
  delay(500);
  
  bufferPos = 0;
  sensorInitialized = true;
  Serial.println("LD2450Manager initialized successfully (Standalone Parser)");
  printConfig();
}

void LD2450Manager::printConfig() {
  Serial.println("\n=== LD2450 Configuration ===");
  Serial.printf("Device Name: %s\n", config.deviceName.c_str());
  Serial.printf("Magic Word: %s\n", config.magicWord.c_str());
  Serial.printf("Detection Range: %d cm\n", config.rangeMaxCm);
  Serial.printf("Debounce Time: %lu ms\n", config.debounceMs);
  Serial.printf("Filter: %s\n", config.filterEnable ? "Enabled" : "Disabled");
  Serial.printf("Sensor: %s\n", config.sensorEnable ? "Enabled" : "Disabled");
  Serial.println("============================\n");
}

bool LD2450Manager::readSensor() {
  if (!sensorInitialized || !config.sensorEnable) {
    return false;
  }
  
  lastReadTime = millis();
  
  // Read all available bytes from UART2
  while (Serial2.available()) {
    uint8_t byte = Serial2.read();
    
    // Collect bytes until we have a complete frame (30 bytes)
    if (bufferPos < 30) {
      frameBuffer[bufferPos] = byte;
      bufferPos++;
      
      // Check if we have a complete frame
      if (bufferPos == 30) {
        // Validate: Header at 0-3, Footer at 28-29
        if (frameBuffer[0] == 0xAA && frameBuffer[1] == 0xFF &&
            frameBuffer[2] == 0x03 && frameBuffer[3] == 0x00 &&
            frameBuffer[28] == 0x55 && frameBuffer[29] == 0xCC) {
          
          // Valid frame - parse it
          int validCount = parseFrame(frameBuffer);
          bufferPos = 0;
          
          // Calculate closest distance
          closestDistanceCm = 600;
          for (int i = 0; i < 3; i++) {
            if (targets[i].valid && targets[i].state == PRESENT) {
              int dist = targets[i].lastDistance;
              if (dist > 0 && dist < closestDistanceCm) {
                closestDistanceCm = dist;
              }
            }
          }
          
          return validCount > 0;
        } else {
          // Invalid frame - shift buffer and try again
          for (int i = 0; i < 29; i++) {
            frameBuffer[i] = frameBuffer[i + 1];
          }
          bufferPos = 29;
        }
      }
    }
  }
  
  return false;
}

// Helper function: Decode coordinate with special sign bit encoding
// Bit 7 of high byte = sign bit (0=negative, 1=positive)
// Bits 0-6 of high byte + all of low byte = magnitude
int16_t LD2450Manager::decodeCoordinate(uint8_t lowByte, uint8_t highByte) {
  // Extract magnitude: (highByte & 0x7F) << 8 | lowByte
  int16_t magnitude = ((highByte & 0x7F) << 8) | lowByte;
  
  // Check sign bit (Bit 7 of high byte)
  // If Bit 7 = 0 → negative, if Bit 7 = 1 → positive
  if ((highByte & 0x80) == 0) {
    return -magnitude;
  }
  return magnitude;
}

// Helper function: Decode speed with sign bit encoding
// Identical to coordinate decoding
// Bit 7 of high byte = direction (0=moving away, 1=approaching)
int16_t LD2450Manager::decodeSpeed(uint8_t lowByte, uint8_t highByte) {
  int16_t magnitude = ((highByte & 0x7F) << 8) | lowByte;
  
  // If Bit 7 = 0 → moving away (negative)
  // If Bit 7 = 1 → approaching (positive)
  if ((highByte & 0x80) == 0) {
    return -magnitude;
  }
  return magnitude;
}

int LD2450Manager::parseFrame(uint8_t* frame) {
  int validCount = 0;
  
  // Frame structure (30 bytes):
  // Byte 0-3:   Header (0xAA 0xFF 0x03 0x00)
  // Byte 4-11:  Target 1 (8 bytes)
  // Byte 12-19: Target 2 (8 bytes)
  // Byte 20-27: Target 3 (8 bytes)
  // Byte 28-29: Footer (0x55 0xCC)
  //
  // Target structure (8 bytes):
  // Byte 0-1: X Position (ESPHome special sign-bit encoding)
  // Byte 2-3: Y Position (ESPHome special sign-bit encoding)
  // Byte 4-5: Speed (ESPHome special sign-bit encoding)
  // Byte 6-7: Resolution (standard little-endian unsigned)
  
  for (int i = 0; i < 3; i++) {
    // Calculate base offset for this target
    int baseOffset = 4 + (i * 8);  // 4, 12, 20
    
    // X Position (baseOffset+0, baseOffset+1)
    // CORRECT: Use ESPHome decoding with Bit 7 = sign bit
    int16_t x = decodeCoordinate(frame[baseOffset + 0], frame[baseOffset + 1]);
    
    // Y Position (baseOffset+2, baseOffset+3)
    int16_t y = decodeCoordinate(frame[baseOffset + 2], frame[baseOffset + 3]);
    
    // Speed (baseOffset+4, baseOffset+5)
    // CORRECT: Use ESPHome decoding with Bit 7 = direction
    int16_t speed = decodeSpeed(frame[baseOffset + 4], frame[baseOffset + 5]);
    
    // Resolution (baseOffset+6, baseOffset+7) - Standard little-endian unsigned
    uint16_t resolution = frame[baseOffset + 6] | (frame[baseOffset + 7] << 8);
    
    bool targetValid = (resolution != 0);
    
    // Update state machine if filtering enabled
    if (config.filterEnable) {
      updateTargetState(i, targetValid);
    } else {
      // No filtering - direct mapping
      targets[i].previousState = targets[i].state;
      targets[i].state = targetValid ? PRESENT : ABSENT;
      targets[i].stateChanged = (targets[i].previousState != targets[i].state);
    }
    
    // Store target data
    if (targetValid) {
      targets[i].lastX = x;
      targets[i].lastY = y;
      targets[i].lastSpeed = speed;
      targets[i].valid = true;
      
      // Calculate distance from x, y (both in mm)
      float dist_mm = sqrtf((float)x * x + (float)y * y);
      targets[i].lastDistance = (int)(dist_mm / 10.0); // Convert to cm
      
      validCount++;
      
      Serial.printf("[LD2450] T%d: x=%6dmm y=%6dmm dist=%3dcm speed=%3dcm/s resolution=%d\n",
        i + 1, x, y, targets[i].lastDistance, speed, resolution);
    } else {
      targets[i].previousState = targets[i].state;
      targets[i].state = ABSENT;
      targets[i].stateChanged = (targets[i].previousState != targets[i].state);
      targets[i].valid = false;
      targets[i].lastDistance = 0;
    }
  }
  
  return validCount;
}

int16_t LD2450Manager::readInt16LE(uint8_t* ptr) {
  // Little Endian: low byte first, high byte second
  return (int16_t)((ptr[1] << 8) | ptr[0]);
}

void LD2450Manager::updateTargetState(int targetIdx, bool sensorDetected) {
  TargetInfo& target = targets[targetIdx];
  unsigned long currentTime = millis();
  
  target.previousState = target.state;
  
  switch (target.state) {
    case ABSENT:
      if (sensorDetected) {
        target.state = DEBOUNCE;
        target.stateChangeTime = currentTime;
        target.stateChanged = false;
      }
      break;
      
    case DEBOUNCE:
      if (sensorDetected) {
        if ((currentTime - target.stateChangeTime) >= config.debounceMs) {
          target.state = PRESENT;
          target.stateChanged = true;
          Serial.printf("Target %d PRESENT (debounce confirmed)\n", targetIdx + 1);
        }
      } else {
        target.state = ABSENT;
      }
      break;
      
    case PRESENT:
      if (!sensorDetected) {
        target.state = DEBOUNCE;
        target.stateChangeTime = currentTime;
        target.stateChanged = false;
      }
      break;
  }
  
  target.stateChanged = (target.previousState != target.state) && 
                        (target.state == PRESENT || target.state == ABSENT);
}

bool LD2450Manager::isTargetPresent(int targetIdx) {
  if (targetIdx < 0 || targetIdx >= 3) return false;
  return targets[targetIdx].state == PRESENT;
}

bool LD2450Manager::hasTargetStateChanged(int targetIdx) {
  if (targetIdx < 0 || targetIdx >= 3) return false;
  return targets[targetIdx].stateChanged;
}

int LD2450Manager::getClosestDistanceCm() {
  return closestDistanceCm;
}

int LD2450Manager::getTargetDistanceCm(int targetIdx) {
  if (targetIdx < 0 || targetIdx >= 3) {
    return 0;
  }
  
  if (targets[targetIdx].valid && targets[targetIdx].state == PRESENT) {
    return targets[targetIdx].lastDistance;
  }
  
  return 0;
}

TargetInfo LD2450Manager::getTargetInfo(int targetIdx) {
  if (targetIdx < 0 || targetIdx >= 3) {
    TargetInfo empty = {ABSENT, ABSENT, 0, 0, 0, 0, false, false};
    return empty;
  }
  return targets[targetIdx];
}

const LD2450Config& LD2450Manager::getConfig() const {
  return config;
}

bool LD2450Manager::isSensorInitialized() const {
  return sensorInitialized;
}

String LD2450Manager::generatePayload() {
  String payload = "{\"d\":\"" + String(config.deviceName.c_str()) + "\"";
  payload += ",\"m\":\"" + String(config.magicWord.c_str()) + "\"";
  
  for (int i = 0; i < 3; i++) {
    payload += ",\"t" + String(i + 1) + "\":";
    payload += (targets[i].state == PRESENT) ? "true" : "false";
    
    payload += ",\"t" + String(i + 1) + "_d\":";
    payload += (targets[i].state == PRESENT) ? String(targets[i].lastDistance) : "0";
  }
  
  payload += ",\"x\":" + String(closestDistanceCm);
  payload += ",\"e\":0}";
  
  return payload;
}

void LD2450Manager::printTargetStatus() {
  Serial.println("\n--- Target Status ---");
  for (int i = 0; i < 3; i++) {
    Serial.printf("Target %d: ", i + 1);
    Serial.printf("State=%s ", 
      targets[i].state == ABSENT ? "ABSENT" : 
      targets[i].state == DEBOUNCE ? "DEBOUNCE" : "PRESENT");
    Serial.printf("Valid=%d ", targets[i].valid);
    if (targets[i].valid) {
      Serial.printf("X=%dmm Y=%dmm Dist=%dcm Speed=%dcm/s", 
        targets[i].lastX, targets[i].lastY, 
        targets[i].lastDistance, targets[i].lastSpeed);
    }
    Serial.println();
  }
  Serial.printf("Closest: %d cm\n", closestDistanceCm);
  Serial.println("--------------------\n");
}

void LD2450Manager::saveToNVS() {
  Preferences prefs;
  
  if (!prefs.begin("ld2450_config", false)) {
    Serial.println("Failed to initialize NVS namespace for saving");
    return;
  }
  
  prefs.putInt("range_max", config.rangeMaxCm);
  prefs.putULong("debounce_ms", config.debounceMs);
  prefs.putBool("filter_enable", config.filterEnable);
  prefs.putBool("sensor_enable", config.sensorEnable);
  prefs.putString("device_name", config.deviceName.c_str());
  prefs.putString("magic_word", config.magicWord.c_str());
  
  prefs.end();
  Serial.println("LD2450 Configuration saved to NVS");
}

void LD2450Manager::loadFromNVS() {
  Preferences prefs;
  
  bool nvsExists = prefs.begin("ld2450_config", true);
  
  if (!nvsExists) {
    Serial.println("No saved LD2450 configuration found - using defaults");
    return;
  }
  
  Serial.println("Loading LD2450 configuration from NVS...");
  
  config.rangeMaxCm = prefs.getInt("range_max", 300);
  config.debounceMs = prefs.getULong("debounce_ms", 2500);
  config.filterEnable = prefs.getBool("filter_enable", true);
  config.sensorEnable = prefs.getBool("sensor_enable", true);
  config.deviceName = prefs.getString("device_name", "LD2450_A").c_str();
  config.magicWord = prefs.getString("magic_word", "LD2450").c_str();
  
  prefs.end();
  
  Serial.println("LD2450 Configuration successfully loaded from NVS");
}

void LD2450Manager::setRangeMaxCm(int cm) {
  if (cm >= 1 && cm <= 600) {
    config.rangeMaxCm = cm;
    Serial.printf("Range set to: %d cm\n", cm);
    saveToNVS();
  }
}

void LD2450Manager::setDebounceMs(unsigned long ms) {
  if (ms >= 500 && ms <= 5000) {
    config.debounceMs = ms;
    Serial.printf("Debounce set to: %lu ms\n", ms);
    saveToNVS();
  }
}

void LD2450Manager::setFilterEnable(bool enable) {
  config.filterEnable = enable;
  Serial.printf("Filter: %s\n", enable ? "Enabled" : "Disabled");
  saveToNVS();
}

void LD2450Manager::setSensorEnable(bool enable) {
  config.sensorEnable = enable;
  Serial.printf("Sensor: %s\n", enable ? "Enabled" : "Disabled");
  saveToNVS();
}

void LD2450Manager::setDeviceName(const String& name) {
  config.deviceName = name.c_str();
  Serial.printf("Device name set to: %s\n", config.deviceName.c_str());
  saveToNVS();
}

void LD2450Manager::setMagicWord(const String& word) {
  config.magicWord = word.c_str();
  Serial.printf("Magic word set to: %s\n", config.magicWord.c_str());
  saveToNVS();
}

bool LD2450Manager::processConfigCommand(const String& jsonString) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, jsonString);
  
  if (error) {
    Serial.print("LD2450 JSON Parse Error: ");
    Serial.println(error.c_str());
    return false;
  }
  
  if (!doc.containsKey("m") || doc["m"].as<String>() != config.magicWord.c_str()) {
    Serial.println("LD2450: Wrong magic word or missing");
    return false;
  }
  
  bool configChanged = false;
  
  if (doc.containsKey("range_cm")) {
    setRangeMaxCm(doc["range_cm"].as<int>());
    configChanged = true;
  }
  
  if (doc.containsKey("debounce_ms")) {
    setDebounceMs(doc["debounce_ms"].as<unsigned long>());
    configChanged = true;
  }
  
  if (doc.containsKey("filter_enable")) {
    setFilterEnable(doc["filter_enable"].as<bool>());
    configChanged = true;
  }
  
  if (doc.containsKey("sensor_enable")) {
    setSensorEnable(doc["sensor_enable"].as<bool>());
    configChanged = true;
  }
  
  if (doc.containsKey("device_name")) {
    setDeviceName(doc["device_name"].as<String>());
    configChanged = true;
  }
  
  if (doc.containsKey("magic_word")) {
    setMagicWord(doc["magic_word"].as<String>());
    configChanged = true;
  }
  
  return configChanged;
}