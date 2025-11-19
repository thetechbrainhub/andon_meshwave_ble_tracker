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
  unsigned long lastMeasurementTime;
  
  // Helper methods for drawing
  void drawStartupScreen();
  void drawMainScreen(const std::string& deviceId, 
                      bool t1Present, int t1Distance,
                      bool t2Present, int t2Distance,
                      bool t3Present, int t3Distance,
                      int closestDistance,
                      int rangeThresholdCm,
                      bool filterEnabled);
  void drawAbsentScreen();
  
  String formatDistance(int distanceCm);
  String formatLastSeen(unsigned long lastMeasurementMs);
  
public:
  DisplayManager(int sdaPin, int sclPin);
  ~DisplayManager();
  
  void init();
  
  // Main display update - call this in loop with target data
  void updateDisplay(const std::string& deviceId,
                     bool t1Present, int t1Distance,
                     bool t2Present, int t2Distance,
                     bool t3Present, int t3Distance,
                     int closestDistance,
                     int rangeThresholdCm,
                     bool filterEnabled);
  
  // Update measurement time
  void updateMeasurementTime();
  
  // Turn on/off
  void turnDisplayOff();
  void turnDisplayOn();
  
  // Status
  bool shouldUpdate();
  void displayStartup();
};

#endif // DISPLAYMANAGER_H