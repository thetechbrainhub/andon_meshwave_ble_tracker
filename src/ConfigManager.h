#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <set>
#include <string>

// ConfigManager class to handle dynamic configuration updates
class ConfigManager {
private:
    // Runtime copies of configuration variables (non-const)
    static int runtime_SCAN_TIME;
    static int runtime_SCAN_INTERVAL;
    static int runtime_SCAN_WINDOW;
    static bool runtime_ACTIVE_SCAN;
    static int runtime_TX_POWER;
    static float runtime_ENVIRONMENTAL_FACTOR;
    static float runtime_DISTANCE_THRESHOLD;
    static float runtime_DISTANCE_CORRECTION;
    static float runtime_PROCESS_NOISE;
    static float runtime_MEASUREMENT_NOISE;
    static int runtime_WINDOW_SIZE;
    static int runtime_BEACON_TIMEOUT_SECONDS;
    
    // ✅ NEW: Runtime GATEWAY_ID
    static std::string runtime_GATEWAY_ID;
    
    // MAC address management
    static std::set<std::string> runtime_mac_addresses;
    static bool runtime_USE_DEVICE_FILTER;
    static String runtime_DEVICE_FILTER;
    
    // Helper functions
    static void rebuildDeviceFilterString();
    static void updateBLEScannerSettings();
    
public:
    // Initialize with default values from Config.h
    static void init();
    
    // Parse and process single JSON configuration command
    static bool processConfigCommand(const String& jsonString);
    
    // Getters for runtime values (to replace Config.h constants)
    static int getScanTime() { return runtime_SCAN_TIME; }
    static int getScanInterval() { return runtime_SCAN_INTERVAL; }
    static int getScanWindow() { return runtime_SCAN_WINDOW; }
    static bool getActiveScan() { return runtime_ACTIVE_SCAN; }
    static int getTxPower() { return runtime_TX_POWER; }
    static float getEnvironmentalFactor() { return runtime_ENVIRONMENTAL_FACTOR; }
    static float getDistanceThreshold() { return runtime_DISTANCE_THRESHOLD; }
    static float getDistanceCorrection() { return runtime_DISTANCE_CORRECTION; }
    static float getProcessNoise() { return runtime_PROCESS_NOISE; }
    static float getMeasurementNoise() { return runtime_MEASUREMENT_NOISE; }
    static int getWindowSize() { return runtime_WINDOW_SIZE; }
    static int getBeaconTimeout() { return runtime_BEACON_TIMEOUT_SECONDS; }
    static bool getUseDeviceFilter() { return runtime_USE_DEVICE_FILTER; }
    static const String& getDeviceFilter() { return runtime_DEVICE_FILTER; }
    
    // ✅ NEW: Getter for runtime GATEWAY_ID
    static const std::string& getGatewayID() { return runtime_GATEWAY_ID; }
    
    // Print current configuration
    static void printCurrentConfig();
    
    // Save/Load configuration (optional - for persistent storage)
    static void saveToNVS();
    static void loadFromNVS();
};

#endif // CONFIGMANAGER_H