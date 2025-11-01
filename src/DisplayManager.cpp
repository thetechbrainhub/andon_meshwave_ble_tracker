#include "DisplayManager.h"
#include "Config.h"

// Constructor
DisplayManager::DisplayManager(int sdaPin, int sclPin) 
  : isDisplayActive(true), lastUpdateTime(0), currentBeaconIndex(0), lastRotationTime(0) {
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
  
  // Show startup screen - Professional splash screen
  
  // Title: BLE TRAC (Large, bold)
  display->setFont(u8g2_font_t0_22_tr);
  display->drawStr(10, 28, "BLE TRAC");
  
  // Company name: NGIS PTE LTD (Smaller)
  display->setFont(u8g2_font_t0_17_tr);
  display->drawStr(12, 42, "NGIS PTE LTD");
  
  // Horizontal line separator
  display->drawLine(0, 44, 128, 44);
  
  // Website: www.ng-is.com (Small, at bottom)
  display->setFont(u8g2_font_tom_thumb_4x6_tr);
  display->drawStr(18, 58, "www.ng-is.com");
  
  display->sendBuffer();
  delay(3000); // Show splash for 3 seconds
  
  Serial.println("Display initialized successfully!");
}

String DisplayManager::formatDistance(float distance) {
  char buffer[10];
  
  if (distance < 1.0) {
    sprintf(buffer, "%.1f m", distance);
  } else if (distance < 10.0) {
    sprintf(buffer, "%.1f m", distance);
  } else {
    sprintf(buffer, "%.0f m", distance);
  }
  
  return String(buffer);
}

String DisplayManager::formatLastSeen(unsigned long lastSeenMs) {
  float seconds = lastSeenMs / 1000.0;
  char buffer[15];
  
  if (seconds < 60) {
    sprintf(buffer, "%.1f s", seconds);
  } else if (seconds < 3600) {
    float minutes = seconds / 60.0;
    sprintf(buffer, "%.1f m", minutes);
  } else {
    float hours = seconds / 3600.0;
    sprintf(buffer, "%.1f h", hours);
  }
  
  return String(buffer);
}

void DisplayManager::drawHeader() {
  // Top header bar - Gateway ID (smaller font)
  display->setFont(u8g2_font_t0_12_tr);
  String headerText = "GATEWAY: " + String(GATEWAY_ID.c_str());
  display->drawStr(2, 10, headerText.c_str());
  display->drawLine(0, 12, 128, 12);
}

void DisplayManager::drawBeaconInfo(const std::string& gatewayId, const std::string& beaconName,
                                     float distance, unsigned long lastSeenMs, bool workerPresent) {
  
  // ===== BEACON NAME (Large, on left) =====
  display->setFont(u8g2_font_t0_17_tr);
  String displayName = beaconName.c_str();
  if (displayName.length() > 12) {
    displayName = displayName.substring(0, 12);
  }
  display->drawStr(2, 28, displayName.c_str());
  
  // ===== SEPARATOR LINE =====
  display->drawLine(0, 30, 128, 30);
  
  // ===== DISTANCE (Large numbers) =====
  display->setFont(u8g2_font_t0_22_tr);
  String distStr = formatDistance(distance);
  display->drawStr(2, 52, distStr.c_str());
  
  // ===== LAST SEEN (Time information) =====
  display->setFont(u8g2_font_t0_12_tr);
  String timeStr = "Last: ";
  timeStr += formatLastSeen(lastSeenMs);
  display->drawStr(2, 62, timeStr.c_str());
}

void DisplayManager::drawNoBeacon() {
  display->setFont(u8g2_font_t0_17_tr);
  display->drawStr(10, 32, "No Beacon");
  display->drawStr(15, 48, "Detected");
  display->setFont(u8g2_font_tom_thumb_4x6_tr);
  display->drawStr(20, 60, "Scanning...");
}

void DisplayManager::drawWorkerGone() {
  display->setFont(u8g2_font_t0_22_tr);
  display->drawStr(15, 32, "TECHNICIAN");
  display->drawStr(25, 50, "GONE");
  
  display->setFont(u8g2_font_tom_thumb_4x6_tr);
  display->drawStr(40, 62, "Alert!");
}

// ✅ NEW: Update beacon rotation index
void DisplayManager::updateBeaconRotation(int totalBeacons) {
  unsigned long currentTime = millis();
  
  // Nur wenn mehrere Beacons vorhanden sind, rotieren
  if (totalBeacons > 1 && (currentTime - lastRotationTime >= 2000)) {
    currentBeaconIndex = (currentBeaconIndex + 1) % totalBeacons;
    lastRotationTime = currentTime;
    Serial.printf("DisplayManager: Rotating to beacon %d of %d\n", currentBeaconIndex + 1, totalBeacons);
  }
}

// ✅ NEW: Get current beacon index
int DisplayManager::getCurrentBeaconIndex() {
  return currentBeaconIndex;
}

void DisplayManager::updateDisplay(const std::string& gatewayId, const std::string& beaconName,
                                   float distance, unsigned long lastSeenMs, bool workerPresent,
                                   bool workerActive) {
  
  if (!display) return;
  
  // Prüfe ob Update nötig ist
  unsigned long currentTime = millis();
  if (currentTime - lastUpdateTime < UPDATE_INTERVAL) {
    return;
  }
  lastUpdateTime = currentTime;
  
  display->clearBuffer();
  display->setDrawColor(1);
  
  if (!workerActive) {
    // Worker ist weg - zeige Alert
    drawWorkerGone();
  } else {
    // Normaler Betrieb
    drawHeader();
    drawBeaconInfo(gatewayId, beaconName, distance, lastSeenMs, workerPresent);
  }
  
  display->sendBuffer();
  isDisplayActive = true;
}

void DisplayManager::displayWorkerGone() {
  if (!display) return;
  
  display->clearBuffer();
  display->setDrawColor(1);
  drawWorkerGone();
  display->sendBuffer();
  isDisplayActive = true;
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