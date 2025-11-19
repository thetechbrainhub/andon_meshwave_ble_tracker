#include "ConfigManager.h"
#include "Config.h"
#include <Preferences.h>
#include <ArduinoJson.h>

// Static variable definitions - minimal, nur für Meshtastic
int ConfigManager::runtime_SCAN_TIME = 5;
int ConfigManager::runtime_SCAN_INTERVAL = 100;
int ConfigManager::runtime_SCAN_WINDOW = 99;
bool ConfigManager::runtime_ACTIVE_SCAN = true;
int ConfigManager::runtime_TX_POWER = -59;
float ConfigManager::runtime_ENVIRONMENTAL_FACTOR = 2.7;
float ConfigManager::runtime_DISTANCE_THRESHOLD = 1.0;
float ConfigManager::runtime_DISTANCE_CORRECTION = -0.5;
float ConfigManager::runtime_PROCESS_NOISE = 0.01;
float ConfigManager::runtime_MEASUREMENT_NOISE = 0.5;
int ConfigManager::runtime_WINDOW_SIZE = 5;
int ConfigManager::runtime_BEACON_TIMEOUT_SECONDS = 10;

std::string ConfigManager::runtime_GATEWAY_ID = "TRAC 001";

std::set<std::string> ConfigManager::runtime_mac_addresses;
bool ConfigManager::runtime_USE_DEVICE_FILTER = false;
String ConfigManager::runtime_DEVICE_FILTER = "";

std::string GATEWAY_ID = "TRAC 001";

void ConfigManager::init() {
    // Minimal init - load from NVS if available
    loadFromNVS();
    
    GATEWAY_ID = runtime_GATEWAY_ID;
    
    Serial.println("ConfigManager initialized");
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
    
    // Minimal processing - nur für Meshtastic Gateway ID
    if (doc.containsKey("CMD") && doc["CMD"].as<String>() == "set_gateway_id") {
        if (doc.containsKey("value")) {
            String newGatewayId = doc["value"].as<String>();
            runtime_GATEWAY_ID = newGatewayId.c_str();
            GATEWAY_ID = newGatewayId.c_str();
            Serial.printf("Gateway ID changed to: %s\n", GATEWAY_ID.c_str());
            configChanged = true;
        }
    }
    
    if (configChanged) {
        saveToNVS();
    }
    
    return configChanged;
}

void ConfigManager::printCurrentConfig() {
    Serial.println("\n=== ConfigManager Status ===");
    Serial.printf("GATEWAY_ID: %s\n", GATEWAY_ID.c_str());
    Serial.println("=============================\n");
}

void ConfigManager::saveToNVS() {
    Preferences prefs;
    
    if (!prefs.begin("ble_config", false)) {
        Serial.println("Failed to initialize NVS namespace for saving");
        return;
    }
    
    prefs.putString("gateway_id", runtime_GATEWAY_ID.c_str());
    
    prefs.end();
    Serial.println("Configuration saved to NVS");
}

void ConfigManager::loadFromNVS() {
    Preferences prefs;
    
    bool nvsExists = prefs.begin("ble_config", true);
    
    if (!nvsExists) {
        Serial.println("No saved configuration found - using defaults");
        return;
    }
    
    Serial.println("Loading saved configuration from NVS...");
    
    runtime_GATEWAY_ID = prefs.getString("gateway_id", "TRAC 001").c_str();
    GATEWAY_ID = runtime_GATEWAY_ID;
    
    prefs.end();
    
    Serial.println("Configuration successfully loaded from NVS");
}