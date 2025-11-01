#include "ConfigManager.h"
#include "Config.h"
#include "BLEScanner.h"
#include <Preferences.h>
#include <ArduinoJson.h>

// Static variable definitions
int ConfigManager::runtime_SCAN_TIME = SCAN_TIME;
int ConfigManager::runtime_SCAN_INTERVAL = SCAN_INTERVAL;
int ConfigManager::runtime_SCAN_WINDOW = SCAN_WINDOW;
bool ConfigManager::runtime_ACTIVE_SCAN = ACTIVE_SCAN;
int ConfigManager::runtime_TX_POWER = TX_POWER;
float ConfigManager::runtime_ENVIRONMENTAL_FACTOR = ENVIRONMENTAL_FACTOR;
float ConfigManager::runtime_DISTANCE_THRESHOLD = DISTANCE_THRESHOLD;
float ConfigManager::runtime_DISTANCE_CORRECTION = DISTANCE_CORRECTION;
float ConfigManager::runtime_PROCESS_NOISE = PROCESS_NOISE;
float ConfigManager::runtime_MEASUREMENT_NOISE = MEASUREMENT_NOISE;
int ConfigManager::runtime_WINDOW_SIZE = WINDOW_SIZE;
int ConfigManager::runtime_BEACON_TIMEOUT_SECONDS = BEACON_TIMEOUT_SECONDS;

// ✅ NEW: Initialize GATEWAY_ID
std::string ConfigManager::runtime_GATEWAY_ID = "TRAC 001";

std::set<std::string> ConfigManager::runtime_mac_addresses;
bool ConfigManager::runtime_USE_DEVICE_FILTER = USE_DEVICE_FILTER;
String ConfigManager::runtime_DEVICE_FILTER = DEVICE_FILTER;

// ✅ NEW: Global GATEWAY_ID variable (defined in Config.h as extern)
std::string GATEWAY_ID = "TRAC 001";

void ConfigManager::init() {
    // Initialize MAC addresses from default filter
    parseDeviceFilter(); // This will populate filteredDevices from DEVICE_FILTER
    
    // Copy the parsed devices to our runtime set
    runtime_mac_addresses.clear();
    for (const auto& mac : filteredDevices) {
        runtime_mac_addresses.insert(mac);
    }
    
    // Load saved configuration from NVS (including GATEWAY_ID)
    loadFromNVS();
    
    // Update global GATEWAY_ID
    GATEWAY_ID = runtime_GATEWAY_ID;
    
    Serial.println("ConfigManager initialized with default values");
    Serial.printf("Gateway ID: %s\n", GATEWAY_ID.c_str());
    printCurrentConfig();
}

bool ConfigManager::processConfigCommand(const String& jsonString) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        Serial.print("JSON Parse Error: ");
        Serial.println(error.c_str());
        return false;
    }
    
    // Check if target field exists and matches our gateway ID
    if (!doc.containsKey("target")) {
        Serial.println("ERROR: Missing 'target' field in configuration command");
        return false;
    }
    
    String targetGateway = doc["target"].as<String>();
    if (targetGateway != GATEWAY_ID.c_str()) {
        Serial.printf("ERROR: Command target '%s' does not match this gateway '%s'\n", 
                     targetGateway.c_str(), GATEWAY_ID.c_str());
        return false;
    }
    
    bool configChanged = false;
    
    // ✅ NEW: Handle gateway_id and set_gateway_id commands
    if (doc.containsKey("CMD") && doc["CMD"].as<String>() == "set_gateway_id") {
        if (doc.containsKey("value")) {
            String newGatewayId = doc["value"].as<String>();
            runtime_GATEWAY_ID = newGatewayId.c_str();
            GATEWAY_ID = newGatewayId.c_str();
            Serial.printf("Gateway ID changed to: %s\n", GATEWAY_ID.c_str());
            configChanged = true;
        } else {
            Serial.println("ERROR: set_gateway_id requires 'value' field");
            return false;
        }
    }
    
    // Process BLE Scan Parameters
    if (doc.containsKey("scan_time")) {
        runtime_SCAN_TIME = doc["scan_time"].as<int>();
        Serial.printf("Updated SCAN_TIME to: %d\n", runtime_SCAN_TIME);
        configChanged = true;
    }
    
    if (doc.containsKey("scan_interval")) {
        runtime_SCAN_INTERVAL = doc["scan_interval"].as<int>();
        Serial.printf("Updated SCAN_INTERVAL to: %d\n", runtime_SCAN_INTERVAL);
        configChanged = true;
    }
    
    if (doc.containsKey("scan_window")) {
        runtime_SCAN_WINDOW = doc["scan_window"].as<int>();
        Serial.printf("Updated SCAN_WINDOW to: %d\n", runtime_SCAN_WINDOW);
        configChanged = true;
    }
    
    if (doc.containsKey("active_scan")) {
        runtime_ACTIVE_SCAN = doc["active_scan"].as<bool>();
        Serial.printf("Updated ACTIVE_SCAN to: %s\n", runtime_ACTIVE_SCAN ? "true" : "false");
        configChanged = true;
    }
    
    // Process RSSI to Meter Parameters
    if (doc.containsKey("tx_power")) {
        runtime_TX_POWER = doc["tx_power"].as<int>();
        Serial.printf("Updated TX_POWER to: %d\n", runtime_TX_POWER);
        configChanged = true;
    }
    
    if (doc.containsKey("env_factor")) {
        runtime_ENVIRONMENTAL_FACTOR = doc["env_factor"].as<float>();
        Serial.printf("Updated ENVIRONMENTAL_FACTOR to: %.2f\n", runtime_ENVIRONMENTAL_FACTOR);
        configChanged = true;
    }
    
    if (doc.containsKey("distance_threshold")) {
        runtime_DISTANCE_THRESHOLD = doc["distance_threshold"].as<float>();
        Serial.printf("Updated DISTANCE_THRESHOLD to: %.2f\n", runtime_DISTANCE_THRESHOLD);
        configChanged = true;
    }
    
    if (doc.containsKey("distance_correction")) {
        runtime_DISTANCE_CORRECTION = doc["distance_correction"].as<float>();
        Serial.printf("Updated DISTANCE_CORRECTION to: %.2f\n", runtime_DISTANCE_CORRECTION);
        configChanged = true;
    }
    
    // Process Filter Parameters
    if (doc.containsKey("process_noise")) {
        runtime_PROCESS_NOISE = doc["process_noise"].as<float>();
        Serial.printf("Updated PROCESS_NOISE to: %.3f\n", runtime_PROCESS_NOISE);
        configChanged = true;
    }
    
    if (doc.containsKey("measurement_noise")) {
        runtime_MEASUREMENT_NOISE = doc["measurement_noise"].as<float>();
        Serial.printf("Updated MEASUREMENT_NOISE to: %.3f\n", runtime_MEASUREMENT_NOISE);
        configChanged = true;
    }
    
    if (doc.containsKey("window_size")) {
        runtime_WINDOW_SIZE = doc["window_size"].as<int>();
        Serial.printf("Updated WINDOW_SIZE to: %d\n", runtime_WINDOW_SIZE);
        configChanged = true;
    }
    
    // Process Beacon Tracking Parameters
    if (doc.containsKey("beacon_timeout")) {
        runtime_BEACON_TIMEOUT_SECONDS = doc["beacon_timeout"].as<int>();
        Serial.printf("Updated BEACON_TIMEOUT_SECONDS to: %d\n", runtime_BEACON_TIMEOUT_SECONDS);
        configChanged = true;
    }
    
    // Process MAC Address Commands
    if (doc.containsKey("mac_add")) {
        String mac = doc["mac_add"].as<String>();
        mac.toLowerCase(); // Normalize to lowercase
        runtime_mac_addresses.insert(mac.c_str());
        rebuildDeviceFilterString();
        Serial.printf("Added MAC address: %s\n", mac.c_str());
        configChanged = true;
    }
    
    if (doc.containsKey("mac_remove")) {
        String mac = doc["mac_remove"].as<String>();
        mac.toLowerCase(); // Normalize to lowercase
        runtime_mac_addresses.erase(mac.c_str());
        rebuildDeviceFilterString();
        Serial.printf("Removed MAC address: %s\n", mac.c_str());
        configChanged = true;
    }
    
    if (doc.containsKey("mac_clear")) {
        if (doc["mac_clear"].as<bool>()) {
            runtime_mac_addresses.clear();
            rebuildDeviceFilterString();
            Serial.println("Cleared all MAC addresses");
            configChanged = true;
        }
    }
    
    if (doc.containsKey("mac_enable")) {
        runtime_USE_DEVICE_FILTER = doc["mac_enable"].as<bool>();
        Serial.printf("MAC filter %s\n", runtime_USE_DEVICE_FILTER ? "enabled" : "disabled");
        configChanged = true;
    }
    
    // Update BLE scanner settings if scan parameters changed
    if (configChanged) {
        updateBLEScannerSettings();
        // Save to persistent storage
        saveToNVS();
    }
    
    return configChanged;
}

void ConfigManager::rebuildDeviceFilterString() {
    runtime_DEVICE_FILTER = "";
    bool first = true;
    
    for (const auto& mac : runtime_mac_addresses) {
        if (!first) {
            runtime_DEVICE_FILTER += ",";
        }
        runtime_DEVICE_FILTER += mac.c_str();
        first = false;
    }
    
    // Update the global filteredDevices set used by the BLE scanner
    filteredDevices.clear();
    for (const auto& mac : runtime_mac_addresses) {
        filteredDevices.insert(mac);
    }
    
    Serial.printf("Updated DEVICE_FILTER to: %s\n", runtime_DEVICE_FILTER.c_str());
}

void ConfigManager::updateBLEScannerSettings() {
    // Note: Some BLE settings might require reinitializing the scanner
    // This depends on the NimBLE implementation
    Serial.println("BLE scanner settings updated (may require restart for some parameters)");
}

void ConfigManager::printCurrentConfig() {
    Serial.println("\n=== Current Configuration ===");
    Serial.printf("GATEWAY_ID: %s\n", GATEWAY_ID.c_str());
    Serial.printf("SCAN_TIME: %d\n", runtime_SCAN_TIME);
    Serial.printf("SCAN_INTERVAL: %d\n", runtime_SCAN_INTERVAL);
    Serial.printf("SCAN_WINDOW: %d\n", runtime_SCAN_WINDOW);
    Serial.printf("ACTIVE_SCAN: %s\n", runtime_ACTIVE_SCAN ? "true" : "false");
    Serial.printf("TX_POWER: %d\n", runtime_TX_POWER);
    Serial.printf("ENVIRONMENTAL_FACTOR: %.2f\n", runtime_ENVIRONMENTAL_FACTOR);
    Serial.printf("DISTANCE_THRESHOLD: %.2f\n", runtime_DISTANCE_THRESHOLD);
    Serial.printf("DISTANCE_CORRECTION: %.2f\n", runtime_DISTANCE_CORRECTION);
    Serial.printf("PROCESS_NOISE: %.3f\n", runtime_PROCESS_NOISE);
    Serial.printf("MEASUREMENT_NOISE: %.3f\n", runtime_MEASUREMENT_NOISE);
    Serial.printf("WINDOW_SIZE: %d\n", runtime_WINDOW_SIZE);
    Serial.printf("BEACON_TIMEOUT_SECONDS: %d\n", runtime_BEACON_TIMEOUT_SECONDS);
    Serial.printf("USE_DEVICE_FILTER: %s\n", runtime_USE_DEVICE_FILTER ? "true" : "false");
    Serial.printf("DEVICE_FILTER: %s\n", runtime_DEVICE_FILTER.c_str());
    Serial.printf("MAC addresses count: %d\n", runtime_mac_addresses.size());
    Serial.println("===============================\n");
}

void ConfigManager::saveToNVS() {
    Preferences prefs;
    
    if (!prefs.begin("ble_config", false)) {
        Serial.println("Failed to initialize NVS namespace for saving");
        return;
    }
    
    // ✅ NEW: Save GATEWAY_ID
    prefs.putString("gateway_id", runtime_GATEWAY_ID.c_str());
    
    prefs.putInt("scan_time", runtime_SCAN_TIME);
    prefs.putInt("scan_interval", runtime_SCAN_INTERVAL);
    prefs.putInt("scan_window", runtime_SCAN_WINDOW);
    prefs.putBool("active_scan", runtime_ACTIVE_SCAN);
    prefs.putInt("tx_power", runtime_TX_POWER);
    prefs.putFloat("env_factor", runtime_ENVIRONMENTAL_FACTOR);
    prefs.putFloat("dist_thresh", runtime_DISTANCE_THRESHOLD);
    prefs.putFloat("dist_corr", runtime_DISTANCE_CORRECTION);
    prefs.putFloat("proc_noise", runtime_PROCESS_NOISE);
    prefs.putFloat("meas_noise", runtime_MEASUREMENT_NOISE);
    prefs.putInt("window_size", runtime_WINDOW_SIZE);
    prefs.putInt("beacon_timeout", runtime_BEACON_TIMEOUT_SECONDS);
    prefs.putBool("use_filter", runtime_USE_DEVICE_FILTER);
    prefs.putString("device_filter", runtime_DEVICE_FILTER);
    
    prefs.end();
    Serial.println("Configuration saved to NVS");
}

void ConfigManager::loadFromNVS() {
    Preferences prefs;
    
    // Try to open NVS namespace - suppress error if it doesn't exist yet
    bool nvsExists = prefs.begin("ble_config", true);  // read-only mode
    
    if (!nvsExists) {
        Serial.println("No saved configuration found - using defaults");
        return; // Use default values that are already set
    }
    
    Serial.println("Loading saved configuration from NVS...");
    
    // ✅ NEW: Load GATEWAY_ID
    runtime_GATEWAY_ID = prefs.getString("gateway_id", "TRAC 001").c_str();
    GATEWAY_ID = runtime_GATEWAY_ID;
    
    runtime_SCAN_TIME = prefs.getInt("scan_time", SCAN_TIME);
    runtime_SCAN_INTERVAL = prefs.getInt("scan_interval", SCAN_INTERVAL);
    runtime_SCAN_WINDOW = prefs.getInt("scan_window", SCAN_WINDOW);
    runtime_ACTIVE_SCAN = prefs.getBool("active_scan", ACTIVE_SCAN);
    runtime_TX_POWER = prefs.getInt("tx_power", TX_POWER);
    runtime_ENVIRONMENTAL_FACTOR = prefs.getFloat("env_factor", ENVIRONMENTAL_FACTOR);
    runtime_DISTANCE_THRESHOLD = prefs.getFloat("dist_thresh", DISTANCE_THRESHOLD);
    runtime_DISTANCE_CORRECTION = prefs.getFloat("dist_corr", DISTANCE_CORRECTION);
    runtime_PROCESS_NOISE = prefs.getFloat("proc_noise", PROCESS_NOISE);
    runtime_MEASUREMENT_NOISE = prefs.getFloat("meas_noise", MEASUREMENT_NOISE);
    runtime_WINDOW_SIZE = prefs.getInt("window_size", WINDOW_SIZE);
    runtime_BEACON_TIMEOUT_SECONDS = prefs.getInt("beacon_timeout", BEACON_TIMEOUT_SECONDS);
    runtime_USE_DEVICE_FILTER = prefs.getBool("use_filter", USE_DEVICE_FILTER);
    runtime_DEVICE_FILTER = prefs.getString("device_filter", DEVICE_FILTER);
    
    prefs.end();
    
    // Parse the loaded device filter
    parseDeviceFilter();
    runtime_mac_addresses.clear();
    for (const auto& mac : filteredDevices) {
        runtime_mac_addresses.insert(mac);
    }
    
    Serial.println("Configuration successfully loaded from NVS");
}