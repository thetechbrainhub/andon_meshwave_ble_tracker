#include <Arduino.h>
#include "Config.h"
#include "BLEScanner.h"
#include "DeviceInfo.h"
#include "BeaconTracker.h"
#include "JsonUtils.h"
#include "MeshtasticComm.h"
#include "ConfigManager.h"
#include "DisplayManager.h"

// Last time JSON was output
unsigned long lastJsonOutput = 0;

// Display Manager Instance
DisplayManager* displayManager = nullptr;

// Tracking für Worker-Status
static bool lastWorkerPresent = false;
static bool workerGoneAlertActive = false;
static unsigned long workerGoneAlertTime = 0;
static constexpr unsigned long WORKER_GONE_ALERT_DURATION = 5000;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize Display Manager first
  displayManager = new DisplayManager(5, 6); // SDA=5, SCL=6
  displayManager->init();
  
  // Initialize ConfigManager
  ConfigManager::init();
  
  // Try to load saved configuration from NVS
  Serial.println("Checking for saved configuration...");
  ConfigManager::loadFromNVS();
  
  // ✅ WICHTIG: Initialize UART DIREKT hier in setup() - wie im funktionierenden Projekt
  // Pin-Zuweisung: RX=GPIO44, TX=GPIO43 (aus Config.h)
  Serial.println("Initializing UART1 for Meshtastic communication...");
  Serial.printf("UART1 Config: RX=GPIO%d, TX=GPIO%d, Baudrate=%d\n", UART_RX_PIN, UART_TX_PIN, UART_BAUD_RATE);
  MeshtasticSerial.begin(UART_BAUD_RATE, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  Serial.println("MeshtasticSerial initialized (UART1 @ 115200 baud)");
  
  // Dann nur noch initMeshtasticComm() für weitere Setup-Ausgaben
  initMeshtasticComm();
  
  Serial.println("\n=====================================================");
  Serial.println("BLE Beacon Scanner mit dynamischer Konfiguration");
  Serial.println("=====================================================");
  Serial.printf("Gateway ID: %s\n", GATEWAY_ID.c_str());
  Serial.println("JSON Command Format: {\"target\":\"" + String(GATEWAY_ID.c_str()) + "\",\"parameter\":value}");
  Serial.printf("Distanz-Schwellenwert: %.1f Meter\n", ConfigManager::getDistanceThreshold());
  Serial.printf("TX Power: %d dBm @ 1m\n", ConfigManager::getTxPower());
  Serial.printf("Environmental Factor: %.1f (%.1f=Freiraum, >2.0=mit Hindernissen)\n", 
                ConfigManager::getEnvironmentalFactor(), 2.0);
  Serial.printf("Distanzkorrektur: %.1f meter\n", ConfigManager::getDistanceCorrection());
  Serial.printf("Gleitender Mittelwert: %d Werte\n", ConfigManager::getWindowSize());
  Serial.printf("JSON-Ausgabe-Intervall: %d ms\n", JSON_OUTPUT_INTERVAL);
  Serial.printf("Beacon Timeout: %d Sekunden\n", ConfigManager::getBeaconTimeout());
  Serial.printf("UART für Meshtastic: TX=%d, RX=%d, Baudrate=%d\n", UART_TX_PIN, UART_RX_PIN, UART_BAUD_RATE);
  Serial.println("Beacon-Verschwinden-Erkennung: Aktiv");
  Serial.println("Gateway Targeting System: Aktiv");
  Serial.println("Dynamische Konfiguration über Meshtastic: Aktiv");
  Serial.println("Display-Integration: Aktiv");
  Serial.println("Bereit zum Empfang von Konfigurationsbefehlen über Meshtastic UART...");
  
  Serial.println("=====================================================");
  
  // Initialize BLE scanner
  bleScanner.init();
  
  Serial.println("BLE Scanner initialisiert.");
  Serial.println("Starte Scannen nach BLE-Geräten in der Umgebung...");
  
  // Initialize tracking variables
  initBeaconTracking();
  
  // Initialize timing
  lastJsonOutput = millis();
}

void loop() {
  // Check for incoming Meshtastic configuration commands
  checkForMeshtasticCommands();
  
  // Reset counter for devices in range
  devicesInRangeCount = 0;
  
  // Start scanning
  int deviceCount = bleScanner.scan();
  
  // Count devices within threshold (using dynamic threshold)
  for (auto const& pair : deviceInfoMap) {
    std::string address = pair.first;
    DeviceInfo device = pair.second;
    
    // Skip devices not in our filter (if filter is active)
    if (!isDeviceInFilter(address)) {
      continue;
    }
    
    if (device.filteredDistance <= ConfigManager::getDistanceThreshold() && 
        (millis() - device.lastSeen) < 30000) {
      devicesInRangeCount++;
    }
  }
  
  // Find and track closest beacon for UART output
  findAndTrackClosestBeacon();
  
  // Print brief summary to serial
  Serial.print("Geräte gefunden: ");
  Serial.print(deviceCount);
  Serial.print(" (");
  Serial.print(devicesInRangeCount);
  Serial.println(" innerhalb Schwellenwert)");
  
  // Print status at regular intervals
  static unsigned long lastStatusPrint = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - lastStatusPrint > 10000) {
    lastStatusPrint = currentMillis;
    
    if (!getCurrentClosestBeaconAddress().empty()) {
      Serial.print("Aktuell verfolgter Beacon: ");
      Serial.print(getCurrentClosestBeaconAddress().c_str());
      Serial.print(" (Distanz: ");
      Serial.print(getCurrentClosestBeaconDistance());
      Serial.println(" m)");
    } else {
      Serial.println("Kein Beacon aktuell verfolgt");
    }
    
    // Show current threshold and gateway info for reference
    Serial.printf("Gateway: %s, Distanz-Schwellenwert: %.2fm\n", GATEWAY_ID.c_str(), ConfigManager::getDistanceThreshold());
  }
  
  // ============ DISPLAY UPDATE LOGIC ============
  if (displayManager) {
    // ✅ Count beacons in range
    int beaconsInRange = 0;
    std::string displayBeaconAddress = "";
    
    for (auto const& pair : deviceInfoMap) {
      std::string address = pair.first;
      DeviceInfo device = pair.second;
      
      if (!isDeviceInFilter(address)) continue;
      
      if (device.filteredDistance <= ConfigManager::getDistanceThreshold() && 
          (millis() - device.lastSeen) < 30000) {
        beaconsInRange++;
      }
    }
    
    // ✅ Update rotation if multiple beacons
    if (beaconsInRange > 1) {
      displayManager->updateBeaconRotation(beaconsInRange);
    } else {
      // Reset rotation index if only 1 or 0 beacons
      if (beaconsInRange == 0) {
        // Kein Beacon gefunden - Display aus
        displayManager->turnDisplayOff();
        workerGoneAlertActive = false;
        lastWorkerPresent = false;
      }
    }
    
    // ✅ Get beacon to display (rotated if multiple)
    int beaconIndex = 0;
    displayBeaconAddress = "";
    
    for (auto const& pair : deviceInfoMap) {
      std::string address = pair.first;
      DeviceInfo device = pair.second;
      
      if (!isDeviceInFilter(address)) continue;
      
      if (device.filteredDistance <= ConfigManager::getDistanceThreshold() && 
          (millis() - device.lastSeen) < 30000) {
        
        if (beaconIndex == displayManager->getCurrentBeaconIndex()) {
          displayBeaconAddress = address;
          break;
        }
        beaconIndex++;
      }
    }
    
    // Display the rotated beacon
    if (!displayBeaconAddress.empty() && beaconsInRange > 0) {
      DeviceInfo& currentBeacon = deviceInfoMap[displayBeaconAddress];
      float lastSeenTime = (millis() - currentBeacon.lastSeen);
      
      bool beaconInRange = (currentBeacon.filteredDistance <= ConfigManager::getDistanceThreshold() && 
                           lastSeenTime < 30000);
      
      if (beaconInRange) {
        bool workerIsPresent = (lastSeenTime < (ConfigManager::getBeaconTimeout() * 1000));
        
        if (workerIsPresent != lastWorkerPresent) {
          lastWorkerPresent = workerIsPresent;
          
          if (!workerIsPresent && !workerGoneAlertActive) {
            workerGoneAlertActive = true;
            workerGoneAlertTime = millis();
            Serial.println("DISPLAY: Technician ABSENT - showing alert for 5 seconds!");
            displayManager->displayWorkerGone();
          }
        }
        
        if (workerGoneAlertActive) {
          unsigned long alertDuration = millis() - workerGoneAlertTime;
          if (alertDuration > WORKER_GONE_ALERT_DURATION) {
            workerGoneAlertActive = false;
            displayManager->turnDisplayOff();
            Serial.println("DISPLAY: Alert finished - display off");
          }
        } else if (workerIsPresent) {
          displayManager->turnDisplayOn();
          displayManager->updateDisplay(
            GATEWAY_ID.c_str(),
            currentBeacon.name,
            currentBeacon.filteredDistance,
            lastSeenTime,
            workerIsPresent
          );
        }
      }
    }
  }
  // ============ END DISPLAY UPDATE ============
  
  // Output detailed JSON at intervals to serial
  if (millis() - lastJsonOutput >= JSON_OUTPUT_INTERVAL) {
    outputDevicesAsJson();
    lastJsonOutput = millis();
  }
  
  // Clear scan results to free memory
  bleScanner.clearResults();
}