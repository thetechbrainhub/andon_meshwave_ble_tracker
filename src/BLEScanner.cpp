#include "BLEScanner.h"
#include "Config.h"
#include "ConfigManager.h"
#include "BeaconTracker.h"

// Global instance
BLEScanner bleScanner;

// Global variables from Config.h
std::set<std::string> filteredDevices;
int devicesInRangeCount = 0;

// Callback implementation
void MyAdvertisedDeviceCallbacks::onResult(NimBLEAdvertisedDevice* advertisedDevice) {
  std::string deviceAddress = advertisedDevice->getAddress().toString();
  
  // Check if device is in our filter (if filter is active)
  if (!isDeviceInFilter(deviceAddress)) {
    // Skip devices not in our filter
    return;
  }
  
  int rssi = advertisedDevice->getRSSI();
  
  // Get or create device info
  DeviceInfo& deviceInfo = deviceInfoMap[deviceAddress];
  
  // Update RSSI
  deviceInfo.rssi = rssi;
  
  // Update time last seen
  deviceInfo.lastSeen = millis();
  
  // Calculate and filter distance
  float rawDistance = rssiToMeters(rssi);
  deviceInfo.rawDistance = rawDistance;
  
  // Apply Kalman filter
  deviceInfo.filteredDistance = deviceInfo.kalmanFilter.update(rawDistance);
  
  // Apply moving average filters
  deviceInfo.avgRssi = deviceInfo.rssiFilter.update(rssi);
  deviceInfo.avgDistance = deviceInfo.distanceFilter.update(deviceInfo.filteredDistance);
  
  // Update device name if available
  if (advertisedDevice->haveName()) {
    deviceInfo.name = advertisedDevice->getName();
  } else if (deviceInfo.name.empty()) {
    deviceInfo.name = "Unknown";
  }
  
  // Try to identify device type based on manufacturer data
  if (advertisedDevice->haveManufacturerData()) {
    std::string strManufacturerData = advertisedDevice->getManufacturerData();
    uint8_t* manufacturerData = (uint8_t*)strManufacturerData.data();
    
    if (strManufacturerData.length() >= 2) {
      uint16_t manufacturerId = manufacturerData[0] | (manufacturerData[1] << 8);
      char idStr[7];
      sprintf(idStr, "0x%04X", manufacturerId);
      deviceInfo.manufacturerId = idStr;
      deviceInfo.manufacturerName = getManufacturerName(manufacturerId).c_str();
    }
  }
  
  // Additional service information if available
  if (advertisedDevice->haveServiceUUID()) {
    deviceInfo.serviceUUID = advertisedDevice->getServiceUUID().toString();
  }
  
  // Check if this device is now closer than our current closest
  if (deviceInfo.filteredDistance <= ConfigManager::getDistanceThreshold()) {
    // Compare with current closest
    if (deviceInfo.filteredDistance < getCurrentClosestBeaconDistance() && 
        deviceAddress != getCurrentClosestBeaconAddress()) {
      // This is a new closest beacon
      setBeaconStatusChanged(true);
    } else if (deviceAddress == getCurrentClosestBeaconAddress()) {
      // This is our current beacon, update the last seen time
      updateLastBeaconSeen();
    }
  }
}

// BLEScanner implementation
BLEScanner::BLEScanner() : pBLEScan(nullptr) {
}

void BLEScanner::init() {
  NimBLEDevice::init("");
  pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
  pBLEScan->setActiveScan(ACTIVE_SCAN);
  pBLEScan->setInterval(SCAN_INTERVAL);
  pBLEScan->setWindow(SCAN_WINDOW);
}

int BLEScanner::scan() {
  NimBLEScanResults results = pBLEScan->start(SCAN_TIME, false);
  return results.getCount();
}

void BLEScanner::clearResults() {
  pBLEScan->clearResults();
}

// Implementation of global functions from Config.h
float rssiToMeters(int rssi) {
  if (rssi == 0) {
    return -1.0; // Invalid RSSI
  }
  
  // Calculate distance using log-distance path loss model
  // d = 10^((TxPower - RSSI)/(10 * n))
  // where n is the path loss exponent
  float ratio = (TX_POWER - rssi) / (10.0 * ENVIRONMENTAL_FACTOR);
  float rawDistance = pow(10.0, ratio);
  
  // Apply correction factor based on empirical calibration
  float correctedDistance = rawDistance + DISTANCE_CORRECTION;
  
  // Ensure we don't have negative distances
  return (correctedDistance > 0) ? correctedDistance : 0.1;
}

bool isDeviceInFilter(const std::string& address) {
  // If filter is not active, accept all devices
  if (!ConfigManager::getUseDeviceFilter()) {
    return true;
  }
  
  // Otherwise, check if the device is in our filtered list
  return filteredDevices.find(address) != filteredDevices.end();
}

// ✅ FINAL FIX: parseDeviceFilter liest aus ConfigManager, nicht aus Config.h!
void parseDeviceFilter() {
  // ✅ WICHTIG: Nutze ConfigManager, nicht USE_DEVICE_FILTER aus Config.h
  if (!ConfigManager::getUseDeviceFilter() || ConfigManager::getDeviceFilter().length() == 0) {
    Serial.println("Device filter is disabled. All devices will be monitored.");
    return;
  }
  
  // Clear previous filter
  filteredDevices.clear();
  
  // ✅ WICHTIG: Lese filterStr aus ConfigManager, nicht aus Config.h!
  String filterStr = ConfigManager::getDeviceFilter();
  int idx = 0;
  int lastIdx = 0;
  
  // Find commas and extract MAC addresses
  while ((idx = filterStr.indexOf(',', lastIdx)) >= 0) {
    String macAddr = filterStr.substring(lastIdx, idx);
    macAddr.trim();  // Remove any whitespace
    if (macAddr.length() > 0) {
      filteredDevices.insert(macAddr.c_str());
      Serial.print("Added to filter: ");
      Serial.println(macAddr);
    }
    lastIdx = idx + 1;
  }
  
  // Add the last MAC address (or only one if no commas)
  String macAddr = filterStr.substring(lastIdx);
  macAddr.trim();
  if (macAddr.length() > 0) {
    filteredDevices.insert(macAddr.c_str());
    Serial.print("Added to filter: ");
    Serial.println(macAddr);
  }
  
  Serial.print("Device filter is active with ");
  Serial.print(filteredDevices.size());
  Serial.println(" device(s).");
}