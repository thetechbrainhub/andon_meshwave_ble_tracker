#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <string>
#include "DeviceInfo.h"

// Display Manager fÃ¼r SH1106 OLED
class DisplayManager {
private:
  U8G2_SH1106_128X64_NONAME_F_SW_I2C* display;
  bool isDisplayActive;
  unsigned long lastUpdateTime;
  static constexpr int UPDATE_INTERVAL = 500; // Update every 500ms
  static constexpr int DISPLAY_TIMEOUT = 3000; // Turn off display after 3 seconds if no beacon
  
  // Helper functions for formatting
  String formatDistance(float distance);
  String formatLastSeen(unsigned long lastSeenMs);
  void drawHeader();
  void drawBeaconInfo(const std::string& gatewayId, const std::string& beaconName, 
                      float distance, unsigned long lastSeenMs, bool workerPresent);
  void drawNoBeacon();
  void drawWorkerGone();
  
public:
  DisplayManager(int sdaPin = 5, int sclPin = 6);
  ~DisplayManager();
  
  void init();
  void updateDisplay(const std::string& gatewayId, const std::string& beaconName, 
                     float distance, unsigned long lastSeenMs, bool workerPresent, 
                     bool workerActive = true);
  void displayWorkerGone();
  void turnDisplayOff();
  void turnDisplayOn();
  bool shouldUpdate();
  
};

#endif // DISPLAYMANAGER_H