#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <string>

#define UPDATE_INTERVAL 500  // Update display every 500ms

class DisplayManager {
  
private:
  U8G2_SH1106_128X64_NONAME_F_SW_I2C* display;
  bool isDisplayActive;
  unsigned long lastUpdateTime;
  
  // ✅ NEW: Beacon rotation variables
  int currentBeaconIndex;
  unsigned long lastRotationTime;
  
  // Helper methods for drawing
  void drawHeader();
  void drawBeaconInfo(const std::string& gatewayId, const std::string& beaconName,
                      float distance, unsigned long lastSeenMs, bool workerPresent);
  void drawNoBeacon();
  void drawWorkerGone();
  
  String formatDistance(float distance);
  String formatLastSeen(unsigned long lastSeenMs);
  
public:
  DisplayManager(int sdaPin, int sclPin);
  ~DisplayManager();
  
  void init();
  void updateDisplay(const std::string& gatewayId, const std::string& beaconName,
                     float distance, unsigned long lastSeenMs, bool workerPresent,
                     bool workerActive = true);
  void displayWorkerGone();
  void turnDisplayOff();
  void turnDisplayOn();
  bool shouldUpdate();
  
  // ✅ NEW: Beacon rotation methods
  void updateBeaconRotation(int totalBeacons);
  int getCurrentBeaconIndex();
};

#endif // DISPLAYMANAGER_H