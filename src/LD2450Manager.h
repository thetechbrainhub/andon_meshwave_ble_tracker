#ifndef LD2450MANAGER_H
#define LD2450MANAGER_H

#include <Arduino.h>
#include <string>

// Target state enum
enum TargetState {
  ABSENT,      // Person not detected
  DEBOUNCE,    // Transition state - waiting for confirmation
  PRESENT      // Person detected and confirmed
};

// Target state tracking
struct TargetInfo {
  TargetState state;
  TargetState previousState;
  unsigned long stateChangeTime;
  int lastX;       // mm
  int lastY;       // mm
  int lastSpeed;   // cm/s
  int lastDistance; // cm
  bool valid;
  bool stateChanged;
};

// Configuration structure
struct LD2450Config {
  int rangeMaxCm;           // 1-600cm detection range
  unsigned long debounceMs; // 500-5000ms debounce time (default 2500)
  bool filterEnable;        // Enable/disable state filtering
  bool sensorEnable;        // Enable/disable sensor
  std::string deviceName;   // Device identifier
  std::string magicWord;    // Configuration magic word
};

class LD2450Manager {
private:
  TargetInfo targets[3];
  LD2450Config config;
  unsigned long lastReadTime;
  bool sensorInitialized;
  int closestDistanceCm;
  
  // Frame parsing
  uint8_t frameBuffer[30];
  int bufferPos;
  
  // Helper functions
  void updateTargetState(int targetIdx, bool sensorDetected);
  int parseFrame(uint8_t* frame);
  int16_t readInt16LE(uint8_t* ptr);
  
  // ESPHome-compatible decoders with special sign-bit encoding
  // Bit 7 of highByte = sign bit (0=negative, 1=positive)
  int16_t decodeCoordinate(uint8_t lowByte, uint8_t highByte);
  int16_t decodeSpeed(uint8_t lowByte, uint8_t highByte);
  
public:
  LD2450Manager();
  
  // Initialization
  void init();
  void loadDefaultConfig();
  void printConfig();
  
  // Reading sensor data
  bool readSensor();
  
  // Configuration
  bool processConfigCommand(const String& jsonString);
  void setRangeMaxCm(int cm);
  void setDebounceMs(unsigned long ms);
  void setFilterEnable(bool enable);
  void setSensorEnable(bool enable);
  void setDeviceName(const String& name);
  void setMagicWord(const String& word);
  
  // Getters
  bool isTargetPresent(int targetIdx);
  bool hasTargetStateChanged(int targetIdx);
  int getClosestDistanceCm();
  int getTargetDistanceCm(int targetIdx);
  TargetInfo getTargetInfo(int targetIdx);
  const LD2450Config& getConfig() const;
  bool isSensorInitialized() const;
  
  // JSON output
  String generatePayload();
  
  // Status
  void printTargetStatus();
  
  // NVS Persistence
  void saveToNVS();
  void loadFromNVS();
};

extern LD2450Manager ld2450Manager;

#endif // LD2450MANAGER_H