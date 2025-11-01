#include "BeaconTracker.h"
#include "Config.h"
#include "DeviceInfo.h"
#include "MeshtasticComm.h"
#include <Arduino.h>

// Beacon Tracking Variablen
static std::string currentClosestBeaconAddress = "";
static float currentClosestBeaconDistance = 999.0;
static bool beaconStatusChanged = false;
static unsigned long lastBeaconUpdate = 0;
static bool beaconDisappearanceReported = false;
static std::set<std::string> currentScanBeacons;

// ✅ NEW: Track previous zone state to detect changes
static std::set<std::string> previousZoneBeacons;
static bool zoneStateChanged = false;

// Getter und Setter Implementierungen
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

// ✅ Check if zone composition changed
bool hasZoneCompositionChanged() {
  // Compare current zone with previous zone
  if (currentScanBeacons.size() != previousZoneBeacons.size()) {
    return true;  // Different number of beacons
  }
  
  // Check if any beacon is different
  for (const auto& beacon : currentScanBeacons) {
    if (previousZoneBeacons.find(beacon) == previousZoneBeacons.end()) {
      return true;  // New beacon in zone
    }
  }
  
  // Check if any beacon left
  for (const auto& beacon : previousZoneBeacons) {
    if (currentScanBeacons.find(beacon) == currentScanBeacons.end()) {
      return true;  // Beacon left zone
    }
  }
  
  return false;  // No change
}

// ✅ Send zone update - beacons entering the zone with present:true
void sendZoneUpdate() {
  if (!hasZoneCompositionChanged()) {
    return;  // No change, don't send
  }
  
  zoneStateChanged = true;
  
  // Find newly entered beacons (in current but not in previous)
  std::set<std::string> newBeacons;
  for (const auto& beacon : currentScanBeacons) {
    if (previousZoneBeacons.find(beacon) == previousZoneBeacons.end()) {
      newBeacons.insert(beacon);  // This beacon just entered
    }
  }
  
  // Find departed beacons (in previous but not in current)
  std::set<std::string> departedBeacons;
  for (const auto& beacon : previousZoneBeacons) {
    if (currentScanBeacons.find(beacon) == currentScanBeacons.end()) {
      departedBeacons.insert(beacon);  // This beacon just left
    }
  }
  
  // Send events for newly entered beacons (present:true)
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
  
  // Send events for departed beacons (present:false)
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
  
  // Update previous state
  previousZoneBeacons = currentScanBeacons;
}

// Find the closest beacon and handle tracking
void findAndTrackClosestBeacon() {
  std::string closestBeaconAddress = "";
  float closestBeaconDistance = 999.0;
  DeviceInfo* closestBeacon = nullptr;
  
  Serial.println("UART-DEBUG: Suche nach dem nächsten Beacon...");
  Serial.println("UART-DEBUG: Aktuell verfolgter Beacon: " + String(currentClosestBeaconAddress.empty() ? "keiner" : currentClosestBeaconAddress.c_str()));
  
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
  
  // ✅ Check if zone composition changed and send updates
  sendZoneUpdate();
  
  if (!currentClosestBeaconAddress.empty()) {
    bool beaconIsVisible = currentScanBeacons.find(currentClosestBeaconAddress) != currentScanBeacons.end();
    
    Serial.print("UART-DEBUG: Verfolgter Beacon ");
    Serial.print(currentClosestBeaconAddress.c_str());
    Serial.print(beaconIsVisible ? " ist sichtbar" : " ist NICHT sichtbar");
    Serial.print(", Verschwinden gemeldet: ");
    Serial.println(beaconDisappearanceReported ? "Ja" : "Nein");
    
    if (!beaconIsVisible && !beaconDisappearanceReported) {
      Serial.println("UART-DEBUG: *** BEACON IST VERSCHWUNDEN! ***");
      Serial.println("UART-DEBUG: Sende finale Benachrichtigung mit present: false");
      
      if (deviceInfoMap.find(currentClosestBeaconAddress) != deviceInfoMap.end()) {
        DeviceInfo& currentBeacon = deviceInfoMap[currentClosestBeaconAddress];
        
        sendBeaconToMeshtastic(currentClosestBeaconAddress, currentBeacon, BEACON_TIMEOUT_SECONDS + 1);
        
        beaconDisappearanceReported = true;
        
        Serial.println("UART-DEBUG: Verschwinden-Nachricht wurde gesendet");
      } else {
        Serial.println("UART-DEBUG: FEHLER: Beacon-Info nicht gefunden!");
      }
    }
    
    if (beaconIsVisible && beaconDisappearanceReported) {
      Serial.println("UART-DEBUG: BEACON IST ZURÜCKGEKEHRT! Sende neue Nachricht mit present: true");
      
      beaconStatusChanged = true;
      beaconDisappearanceReported = false;
      
      if (deviceInfoMap.find(currentClosestBeaconAddress) != deviceInfoMap.end()) {
        DeviceInfo& currentBeacon = deviceInfoMap[currentClosestBeaconAddress];
        sendBeaconToMeshtastic(currentClosestBeaconAddress, currentBeacon, 0.0f);
        beaconStatusChanged = false;
        Serial.println("UART-DEBUG: Rückkehr-Nachricht wurde gesendet");
      }
    }
  }
  
  if (closestBeacon != nullptr) {
    if (!currentClosestBeaconAddress.empty() && currentClosestBeaconAddress != closestBeaconAddress) {
      Serial.print("UART-DEBUG: Neuer nächster Beacon gefunden. Alt: ");
      Serial.print(currentClosestBeaconAddress.c_str());
      Serial.print(" -> Neu: ");
      Serial.println(closestBeaconAddress.c_str());
      
      beaconStatusChanged = true;
      currentClosestBeaconAddress = closestBeaconAddress;
      currentClosestBeaconDistance = closestBeaconDistance;
      lastBeaconUpdate = millis();
      
      beaconDisappearanceReported = false;
      Serial.println("UART-DEBUG: Verschwinden-Flag zurückgesetzt für neuen Beacon");
      
    } else if (currentClosestBeaconAddress.empty()) {
      Serial.println("UART-DEBUG: Erster Beacon entdeckt!");
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
    Serial.println("UART-DEBUG: Kein Beacon innerhalb des Schwellenwerts gefunden.");
    
    if (!currentClosestBeaconAddress.empty() && currentScanBeacons.empty() && !beaconDisappearanceReported) {
      Serial.println("UART-DEBUG: Alle Beacons sind verschwunden!");
      
      if (deviceInfoMap.find(currentClosestBeaconAddress) != deviceInfoMap.end()) {
        DeviceInfo& lastTrackedBeacon = deviceInfoMap[currentClosestBeaconAddress];
        
        Serial.println("UART-DEBUG: Sende finale Benachrichtigung für letzten Beacon mit present: false");
        sendBeaconToMeshtastic(currentClosestBeaconAddress, lastTrackedBeacon, BEACON_TIMEOUT_SECONDS + 1);
        
        beaconDisappearanceReported = true;
        Serial.println("UART-DEBUG: Verschwinden-Nachricht wurde gesendet");
      }
    }
  }
  
  Serial.print("UART-DEBUG: Aktuell sichtbare Beacons: ");
  Serial.println(currentScanBeacons.size());
  Serial.print("UART-DEBUG: Verfolgter Beacon: ");
  Serial.println(currentClosestBeaconAddress.empty() ? "keiner" : currentClosestBeaconAddress.c_str());
  Serial.print("UART-DEBUG: Status-Change-Flag: ");
  Serial.println(beaconStatusChanged ? "Ja" : "Nein");
  Serial.print("UART-DEBUG: Verschwinden bereits gemeldet: ");
  Serial.println(beaconDisappearanceReported ? "Ja" : "Nein");
  Serial.println("UART-DEBUG: Ende der Beacon-Tracking-Funktion");
}