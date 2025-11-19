#include "DisplayManager.h"
#include "Config.h"

// Constructor
DisplayManager::DisplayManager(int sdaPin, int sclPin) 
  : isDisplayActive(true), lastUpdateTime(0), lastMeasurementTime(0) {
  display = new U8G2_SH1106_128X64_NONAME_F_SW_I2C(U8G2_R0, sclPin, sdaPin, U8X8_PIN_NONE);
}

// Destructor
DisplayManager::~DisplayManager() {
  if (display) {
    delete display;
  }
}

void DisplayManager::init() {
  if (!display) return;
  
  Serial.println("Initializing display...");
  display->begin();
  display->setContrast(200);
  display->clearBuffer();
  display->setDrawColor(1);
  
  displayStartup();
  
  Serial.println("Display initialized successfully!");
}

void DisplayManager::displayStartup() {
  if (!display) return;
  
  display->clearBuffer();
  display->setDrawColor(1);
  
  // MeshWave - centered
  display->setFont(u8g2_font_t0_22_tr);
  int x1 = (128 - display->getStrWidth("MeshWave")) / 2;
  display->drawStr(x1, 20, "MeshWave");
  
  // mmWave - centered
  int x2 = (128 - display->getStrWidth("mmWave")) / 2;
  display->drawStr(x2, 40, "mmWave");
  
  // NGIS PTE LTD - centered
  display->setFont(u8g2_font_t0_12_tr);
  int x3 = (128 - display->getStrWidth("NGIS PTE LTD")) / 2;
  display->drawStr(x3, 58, "NGIS PTE LTD");
  
  display->sendBuffer();
  delay(3000);
}

String DisplayManager::formatDistance(int distanceCm) {
  if (distanceCm >= 600 || distanceCm <= 0) {
    return "---";
  }
  
  char buffer[10];
  sprintf(buffer, "%d", distanceCm);
  return String(buffer);
}

String DisplayManager::formatLastSeen(unsigned long lastMeasurementMs) {
  if (lastMeasurementMs == 0) {
    return "0.0s";
  }
  
  float seconds = lastMeasurementMs / 1000.0;
  char buffer[15];
  
  if (seconds < 60) {
    sprintf(buffer, "%.1f", seconds);
  } else {
    float minutes = seconds / 60.0;
    sprintf(buffer, "%.1fM", minutes);
  }
  
  return String(buffer);
}

void DisplayManager::updateDisplay(const std::string& deviceId,
                                   bool t1Present, int t1Distance,
                                   bool t2Present, int t2Distance,
                                   bool t3Present, int t3Distance,
                                   int closestDistance,
                                   int rangeThresholdCm,
                                   bool filterEnabled) {
  if (!display) return;
  
  // Check if any target present
  bool anyTargetPresent = t1Present || t2Present || t3Present;
  
  // Turn off display if no target
  if (!anyTargetPresent) {
    turnDisplayOff();
    return;
  }
  
  // Turn on display if target detected
  turnDisplayOn();
  
  // Check if update needed
  unsigned long currentTime = millis();
  if (currentTime - lastUpdateTime < UPDATE_INTERVAL) {
    return;
  }
  lastUpdateTime = currentTime;
  
  display->clearBuffer();
  display->setDrawColor(1);
  
  drawMainScreen(deviceId, t1Present, t1Distance, t2Present, t2Distance, 
                 t3Present, t3Distance, closestDistance, rangeThresholdCm, filterEnabled);
  
  display->sendBuffer();
  isDisplayActive = true;
}

void DisplayManager::drawMainScreen(const std::string& deviceId,
                                    bool t1Present, int t1Distance,
                                    bool t2Present, int t2Distance,
                                    bool t3Present, int t3Distance,
                                    int closestDistance,
                                    int rangeThresholdCm,
                                    bool filterEnabled) {
  // Line 1: ID: DeviceName
  display->setFont(u8g2_font_t0_12_tr);
  String line1 = "ID: " + String(deviceId.c_str());
  display->drawStr(0, 10, line1.c_str());
  
  // Trennstrich unter ID
  display->drawLine(0, 12, 128, 12);
  
  // Line 2: T1: IN/OUT  T2: IN/OUT  T3: IN/OUT
  String t1Status = t1Present ? "IN" : "OUT";
  String t2Status = t2Present ? "IN" : "OUT";
  String t3Status = t3Present ? "IN" : "OUT";
  
  String line2 = "T1:" + t1Status + "  T2:" + t2Status + "  T3:" + t3Status;
  display->drawStr(0, 25, line2.c_str());
  
  // Line 3: distances aligned under status
  String t1Dist = t1Present ? String(t1Distance) + "cm" : "---";
  String t2Dist = t2Present ? String(t2Distance) + "cm" : "---";
  String t3Dist = t3Present ? String(t3Distance) + "cm" : "---";
  
  String line3 = t1Dist + "    " + t2Dist + "    " + t3Dist;
  display->drawStr(0, 37, line3.c_str());
  
  // Trennstrich
  display->drawLine(0, 40, 128, 40);
  
  // Line 4: Range threshold
  String line4 = "Range: " + String(rangeThresholdCm) + "cm";
  display->drawStr(0, 51, line4.c_str());
  
  // Line 5: Filter status and Magic Word
  String filterStr = filterEnabled ? "ON" : "OFF";
  String line5 = "Filter:" + filterStr + " MW:LD2450";
  display->drawStr(0, 62, line5.c_str());
}

void DisplayManager::drawAbsentScreen() {
  // No Target - centered
  display->setFont(u8g2_font_t0_17_tr);
  int x1 = (128 - display->getStrWidth("No Target")) / 2;
  display->drawStr(x1, 32, "No Target");
  
  // Detected - centered
  int x2 = (128 - display->getStrWidth("Detected")) / 2;
  display->drawStr(x2, 48, "Detected");
  
  // Scanning... - centered
  display->setFont(u8g2_font_tom_thumb_4x6_tr);
  int x3 = (128 - display->getStrWidth("Scanning...")) / 2;
  display->drawStr(x3, 60, "Scanning...");
}

void DisplayManager::updateMeasurementTime() {
  lastMeasurementTime = millis();
}

void DisplayManager::turnDisplayOff() {
  if (!display || !isDisplayActive) return;
  
  display->clearBuffer();
  display->sendBuffer();
  display->setContrast(0);
  isDisplayActive = false;
}

void DisplayManager::turnDisplayOn() {
  if (!display || isDisplayActive) return;
  
  display->setContrast(200);
  isDisplayActive = true;
}

bool DisplayManager::shouldUpdate() {
  unsigned long currentTime = millis();
  return (currentTime - lastUpdateTime >= UPDATE_INTERVAL);
}