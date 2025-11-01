#include "JsonUtils.h"
#include "Config.h"
#include "DeviceInfo.h"
#include "BeaconTracker.h"

// Generate JSON for a specific beacon
String generateBeaconJSON(const std::string& address, DeviceInfo& device, float lastSeenOverride) {
  // Berechne last_seen Wert
  float lastSeenValue = 0;
  bool forceCrusherAbsent = false;
  
  if (lastSeenOverride >= 0) {
    lastSeenValue = lastSeenOverride;
    // Wenn lastSeenOverride > BEACON_TIMEOUT_SECONDS, setzen wir beacon auf false
    if (lastSeenOverride > BEACON_TIMEOUT_SECONDS) {
      forceCrusherAbsent = true;
    }
  } else {
    lastSeenValue = (millis() - device.lastSeen) / 1000.0;
  }
  
  // Bestimme beacon basierend auf last_seen im Vergleich zum Schwellenwert
  // Aber überschreibe mit forceCrusherAbsent, wenn gesetzt
  bool isBeaconPresent = forceCrusherAbsent ? false : (lastSeenValue < BEACON_TIMEOUT_SECONDS);
  
  // Debug-Ausgabe zur JSON-Generierung
  Serial.println("UART-DEBUG: JSON-Generierung für Beacon: " + String(address.c_str()));
  Serial.print("UART-DEBUG: last_seen = "); 
  Serial.print(lastSeenValue);
  Serial.print(", forceCrusherAbsent = ");
  Serial.print(forceCrusherAbsent ? "true" : "false");
  Serial.print(", beacon = ");
  Serial.println(isBeaconPresent ? "true" : "false");
  
  // Generiere JSON
  String json = "{";
  json += "\"name\":\"" + String(device.name.c_str()) + "\",";
  json += "\"distance\":" + String(device.filteredDistance, 2) + ",";
  json += "\"last_seen\":" + String(lastSeenValue, 1) + ",";
  json += "\"beacon\":" + String(isBeaconPresent ? "true" : "false");
  json += "}";
  
  return json;
}

// Generate JSON string for all devices within threshold
String generateDevicesJSON() {
  String json = "{\"devices\":[";
  
  bool firstDevice = true;
  int deviceCount = 0;
  
  for (auto const& pair : deviceInfoMap) {
    std::string address = pair.first;
    DeviceInfo device = pair.second;
    
    // Skip devices not in our filter (if filter is active)
    if (!isDeviceInFilter(address)) {
      continue;
    }
    
    // Only include devices within range and seen in the last 30 seconds
    if (device.filteredDistance <= DISTANCE_THRESHOLD && (millis() - device.lastSeen) < 30000) {
      // Add comma separator between devices
      if (!firstDevice) {
        json += ",";
      }
      firstDevice = false;
      deviceCount++;
      
      // Output device as JSON object
      json += generateBeaconJSON(address, device);
    }
  }
  
  json += "],";
  json += "\"count\":" + String(deviceCount) + ",";
  json += "\"timestamp\":" + String(millis() / 1000.0);
  json += "}";
  
  return json;
}

// Output JSON formatted device data to Serial
void outputDevicesAsJson() {
  // Generate JSON string for serial output
  String jsonStr = generateDevicesJSON();
  
  // Print to serial
  Serial.println("Device data:");
  Serial.println(jsonStr);
}