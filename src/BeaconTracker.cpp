#include "BeaconTracker.h"
#include "Config.h"
#include "DeviceInfo.h"
#include "MeshtasticComm.h"
#include "DisplayManager.h"
#include <Arduino.h>

// ✅ Global DisplayManager reference
static class DisplayManager* g_displayManager = nullptr;

void setDisplayManager(class DisplayManager* dm) {
  g_displayManager = dm;
  Serial.println("BEACON-TRACKER: DisplayManager reference set");
}

// Beacon Tracking Variables
static std::string currentClosestBeaconAddress = "";
static float currentClosestBeaconDistance = 999.0;
static bool beaconStatusChanged = false;
static unsigned long lastBeaconUpdate = 0;
static bool beaconDisappearanceReported = false;
static std::set<std::string> currentScanBeacons;
static std::set<std::string> previousZoneBeacons;
static bool zoneStateChanged = false;

// Getter implementations
const std::string& getCurrentClosestBeaconAddress() {
  return currentClosestBeaconAddress;
}

float getCurrentClosestBeaconDistance() {
  return currentClosestBeaconDistance;
}

bool getBeaconStatusChanged() {
  return beaconStatusChanged;
}

bool getBeaconDisappearanceReported() {
  return beaconDisappearanceReported;
}

std::set<std::string>& getCurrentScanBeacons() {
  return currentScanBeacons;
}

// Setter implementations
void setCurrentClosestBeaconAddress(const std::string& address) {
  currentClosestBeaconAddress = address;
}

void setCurrentClosestBeaconDistance(float distance) {
  currentClosestBeaconDistance = distance;
}

void setBeaconStatusChanged(bool changed) {
  beaconStatusChanged = changed;
}

void setBeaconDisappearanceReported(bool reported) {
  beaconDisappearanceReported = reported;
}

void clearCurrentScanBeacons() {
  currentScanBeacons.clear();
}

void updateLastBeaconSeen() {
  lastBeaconUpdate = millis();
}

unsigned long getLastBeaconUpdate() {
  return lastBeaconUpdate;
}

void initBeaconTracking() {
  currentClosestBeaconAddress = "";
  currentClosestBeaconDistance = 999.0;
  beaconStatusChanged = false;
  beaconDisappearanceReported = false;
  currentScanBeacons.clear();
  previousZoneBeacons.clear();
  lastBeaconUpdate = 0;
}

bool hasZoneCompositionChanged() {
  if (currentScanBeacons.size() != previousZoneBeacons.size()) {
    return true;
  }
  
  for (const auto& beacon : currentScanBeacons) {
    if (previousZoneBeacons.find(beacon) == previousZoneBeacons.end()) {
      return true;
    }
  }
  
  for (const auto& beacon : previousZoneBeacons) {
    if (currentScanBeacons.find(beacon) == currentScanBeacons.end()) {
      return true;
    }
  }
  
  return false;
}

void sendZoneUpdate() {
  if (!hasZoneCompositionChanged()) {
    return;
  }
  
  zoneStateChanged = true;
  
  std::set<std::string> newBeacons;
  for (const auto& beacon : currentScanBeacons) {
    if (previousZoneBeacons.find(beacon) == previousZoneBeacons.end()) {
      newBeacons.insert(beacon);
    }
  }
  
  std::set<std::string> departedBeacons;
  for (const auto& beacon : previousZoneBeacons) {
    if (currentScanBeacons.find(beacon) == currentScanBeacons.end()) {
      departedBeacons.insert(beacon);
    }
  }
  
  for (const auto& address : newBeacons) {
    if (deviceInfoMap.find(address) != deviceInfoMap.end()) {
      DeviceInfo& device = deviceInfoMap[address];
      String json = "{\"zone\":\"" + String(GATEWAY_ID.c_str()) + "\",\"n\":\"" + String(device.name.c_str()) + "\",\"d\":" + String(device.filteredDistance, 2) + ",\"present\":true}";
      
      Serial.println("UART-DEBUG: Device ENTERED zone - sending presence notification:");
      Serial.println(json);
      Serial.printf("JSON size: %d bytes\n", json.length());
      
      MeshtasticSerial.println(json);
    }
  }
  
  for (const auto& address : departedBeacons) {
    if (deviceInfoMap.find(address) != deviceInfoMap.end()) {
      DeviceInfo& device = deviceInfoMap[address];
      String json = "{\"zone\":\"" + String(GATEWAY_ID.c_str()) + "\",\"n\":\"" + String(device.name.c_str()) + "\",\"present\":false}";
      
      Serial.println("UART-DEBUG: Device LEFT zone - sending absence notification:");
      Serial.println(json);
      Serial.printf("JSON size: %d bytes\n", json.length());
      
      MeshtasticSerial.println(json);
    }
  }
  
  previousZoneBeacons = currentScanBeacons;
  
  // Check if current beacon still in zone and update display
  if (!currentClosestBeaconAddress.empty()) {
    bool currentBeaconStillInZone = currentScanBeacons.find(currentClosestBeaconAddress) != currentScanBeacons.end();
    
    if (!currentBeaconStillInZone) {
      Serial.println("UART-DEBUG: Currently tracked beacon left the zone!");
      
      currentClosestBeaconAddress = "";
      currentClosestBeaconDistance = 999.0;
      
      for (const auto& address : currentScanBeacons) {
        if (deviceInfoMap.find(address) != deviceInfoMap.end()) {
          DeviceInfo& device = deviceInfoMap[address];
          if (device.filteredDistance < currentClosestBeaconDistance) {
            currentClosestBeaconDistance = device.filteredDistance;
            currentClosestBeaconAddress = address;
          }
        }
      }
      
      if (!currentClosestBeaconAddress.empty()) {
        Serial.println("UART-DEBUG: Switched to new closest beacon: " + String(currentClosestBeaconAddress.c_str()));
        beaconDisappearanceReported = false;
        if (g_displayManager) {
          g_displayManager->resetBeaconIndex();
        }
      } else {
        Serial.println("UART-DEBUG: No beacons left in zone");
        beaconDisappearanceReported = false;
        if (g_displayManager) {
          g_displayManager->resetBeaconIndex();
        }
      }
    }
  }
}

void findAndTrackClosestBeacon() {
  std::string closestBeaconAddress = "";
  float closestBeaconDistance = 999.0;
  DeviceInfo* closestBeacon = nullptr;
  
  Serial.println("UART-DEBUG: Searching for closest beacon...");
  Serial.println("UART-DEBUG: Currently tracked beacon: " + String(currentClosestBeaconAddress.empty() ? "none" : currentClosestBeaconAddress.c_str()));
  
  currentScanBeacons.clear();
  
  for (auto& pair : deviceInfoMap) {
    std::string address = pair.first;
    DeviceInfo& device = pair.second;
    
    if (!isDeviceInFilter(address)) {
      continue;
    }
    
    if (device.filteredDistance <= DISTANCE_THRESHOLD && 
        (millis() - device.lastSeen) < 30000) {
      
      currentScanBeacons.insert(address);
      
      if (device.filteredDistance < closestBeaconDistance) {
        closestBeaconDistance = device.filteredDistance;
        closestBeaconAddress = address;
        closestBeacon = &device;
      }
    }
  }
  
  sendZoneUpdate();
  
  if (!currentClosestBeaconAddress.empty()) {
    bool beaconIsVisible = currentScanBeacons.find(currentClosestBeaconAddress) != currentScanBeacons.end();
    
    Serial.print("UART-DEBUG: Tracked beacon ");
    Serial.print(currentClosestBeaconAddress.c_str());
    Serial.print(beaconIsVisible ? " is visible" : " is NOT visible");
    Serial.print(", disappearance reported: ");
    Serial.println(beaconDisappearanceReported ? "Yes" : "No");
    
    if (!beaconIsVisible && !beaconDisappearanceReported) {
      Serial.println("UART-DEBUG: *** BEACON DISAPPEARED! ***");
      Serial.println("UART-DEBUG: Sending final notification with present: false");
      
      if (deviceInfoMap.find(currentClosestBeaconAddress) != deviceInfoMap.end()) {
        DeviceInfo& currentBeacon = deviceInfoMap[currentClosestBeaconAddress];
        
        sendBeaconToMeshtastic(currentClosestBeaconAddress, currentBeacon, BEACON_TIMEOUT_SECONDS + 1);
        
        beaconDisappearanceReported = true;
        
        Serial.println("UART-DEBUG: Disappearance notification sent");
      } else {
        Serial.println("UART-DEBUG: ERROR: Beacon info not found!");
      }
    }
    
    if (beaconIsVisible && beaconDisappearanceReported) {
      Serial.println("UART-DEBUG: BEACON RETURNED! Sending new notification with present: true");
      
      beaconStatusChanged = true;
      beaconDisappearanceReported = false;
      
      if (deviceInfoMap.find(currentClosestBeaconAddress) != deviceInfoMap.end()) {
        DeviceInfo& currentBeacon = deviceInfoMap[currentClosestBeaconAddress];
        sendBeaconToMeshtastic(currentClosestBeaconAddress, currentBeacon, 0.0f);
        beaconStatusChanged = false;
        Serial.println("UART-DEBUG: Return notification sent");
      }
    }
  }
  
  if (closestBeacon != nullptr) {
    if (!currentClosestBeaconAddress.empty() && currentClosestBeaconAddress != closestBeaconAddress) {
      Serial.print("UART-DEBUG: New closest beacon found. Old: ");
      Serial.print(currentClosestBeaconAddress.c_str());
      Serial.print(" -> New: ");
      Serial.println(closestBeaconAddress.c_str());
      
      beaconStatusChanged = true;
      currentClosestBeaconAddress = closestBeaconAddress;
      currentClosestBeaconDistance = closestBeaconDistance;
      lastBeaconUpdate = millis();
      
      beaconDisappearanceReported = false;
      Serial.println("UART-DEBUG: Disappearance flag reset for new beacon");
      
    } else if (currentClosestBeaconAddress.empty()) {
      Serial.println("UART-DEBUG: First beacon detected!");
      beaconStatusChanged = true;
      currentClosestBeaconAddress = closestBeaconAddress;
      currentClosestBeaconDistance = closestBeaconDistance;
      lastBeaconUpdate = millis();
      
      beaconDisappearanceReported = false;
    } 
    else if (currentClosestBeaconAddress == closestBeaconAddress) {
      currentClosestBeaconDistance = closestBeaconDistance;
      lastBeaconUpdate = millis();
    }
  } else {
    Serial.println("UART-DEBUG: No beacon within threshold found.");
    
    if (!currentClosestBeaconAddress.empty() && currentScanBeacons.empty() && !beaconDisappearanceReported) {
      Serial.println("UART-DEBUG: All beacons disappeared!");
      
      if (deviceInfoMap.find(currentClosestBeaconAddress) != deviceInfoMap.end()) {
        DeviceInfo& lastTrackedBeacon = deviceInfoMap[currentClosestBeaconAddress];
        
        Serial.println("UART-DEBUG: Sending final notification for last beacon with present: false");
        sendBeaconToMeshtastic(currentClosestBeaconAddress, lastTrackedBeacon, BEACON_TIMEOUT_SECONDS + 1);
        
        beaconDisappearanceReported = true;
        Serial.println("UART-DEBUG: Disappearance notification sent");
      }
    }
  }
  
  Serial.print("UART-DEBUG: Currently visible beacons: ");
  Serial.println(currentScanBeacons.size());
  Serial.print("UART-DEBUG: Tracked beacon: ");
  Serial.println(currentClosestBeaconAddress.empty() ? "none" : currentClosestBeaconAddress.c_str());
  Serial.print("UART-DEBUG: Status-change flag: ");
  Serial.println(beaconStatusChanged ? "Yes" : "No");
  Serial.print("UART-DEBUG: Disappearance already reported: ");
  Serial.println(beaconDisappearanceReported ? "Yes" : "No");
  Serial.println("UART-DEBUG: End of beacon tracking function");
}