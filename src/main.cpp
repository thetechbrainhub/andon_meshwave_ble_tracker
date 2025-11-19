#include <Arduino.h>
#include "LD2450Manager.h"
#include "MeshtasticComm.h"
#include "ConfigManager.h"
#include "DisplayManager.h"

// Display Manager Instance
DisplayManager* displayManager = nullptr;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=====================================================");
  Serial.println("LD2450 mmWave Sensor - Mechaniker Tracking");
  Serial.println("=====================================================\n");
  
  // Initialize Display Manager
  displayManager = new DisplayManager(5, 6); // SDA=5, SCL=6
  displayManager->init();
  
  // Initialize ConfigManager for Meshtastic config
  ConfigManager::init();
  ConfigManager::loadFromNVS();
  
  // Initialize UART1 for Meshtastic (TX=GPIO43, RX=GPIO44, 115200 baud)
  Serial.println("Initializing UART1 for Meshtastic...");
  Serial1.begin(115200, SERIAL_8N1, 44, 43);
  Serial.println("UART1 initialized (Meshtastic @ 115200 baud)");
  initMeshtasticComm();
  
  // Initialize LD2450 Sensor Manager
  // Uses UART2 with Standalone Binary Protocol Parser
  Serial.println("Initializing LD2450 Sensor...");
  ld2450Manager.init();
  
  Serial.println("=====================================================");
  Serial.println("System ready. Waiting for sensor data and commands...");
  Serial.println("=====================================================\n");
}

void loop() {
  // Check for incoming Meshtastic configuration commands
  checkForMeshtasticCommands();
  
  // Read LD2450 sensor data
  if (ld2450Manager.readSensor()) {
    // Sensor returned data - check for state changes
    for (int i = 0; i < 3; i++) {
      if (ld2450Manager.hasTargetStateChanged(i)) {
        Serial.printf("Target %d state changed to: %s\n", 
          i + 1, 
          ld2450Manager.isTargetPresent(i) ? "PRESENT" : "ABSENT");
      }
    }
  }
  
  // Send payload only when state changes (after debounce)
  bool anyStateChanged = false;
  for (int i = 0; i < 3; i++) {
    if (ld2450Manager.hasTargetStateChanged(i)) {
      anyStateChanged = true;
      break;
    }
  }
  
  if (anyStateChanged) {
    String payload = ld2450Manager.generatePayload();
    
    Serial.println(">>> State changed! Sending payload:");
    Serial.println(payload);
    Serial.printf("Payload size: %d bytes\n\n", payload.length());
    
    // Send via Meshtastic
    Serial1.println(payload);
  }
  
  // Print detailed status periodically
  static unsigned long lastStatusPrint = 0;
  if (millis() - lastStatusPrint > 10000) {
    ld2450Manager.printTargetStatus();
    lastStatusPrint = millis();
  }
  
  // Update display if available
  if (displayManager) {
    // Get all target info
    bool t1Present = ld2450Manager.isTargetPresent(0);
    bool t2Present = ld2450Manager.isTargetPresent(1);
    bool t3Present = ld2450Manager.isTargetPresent(2);
    
    // Get INDIVIDUAL distances for each target (in cm)
    int t1DistCm = ld2450Manager.getTargetDistanceCm(0);
    int t2DistCm = ld2450Manager.getTargetDistanceCm(1);
    int t3DistCm = ld2450Manager.getTargetDistanceCm(2);
    
    // Update display with all target info
    displayManager->updateDisplay(
      ld2450Manager.getConfig().deviceName,
      t1Present, t1DistCm,
      t2Present, t2DistCm,
      t3Present, t3DistCm,
      ld2450Manager.getClosestDistanceCm(),
      ld2450Manager.getConfig().rangeMaxCm,
      ld2450Manager.getConfig().filterEnable
    );
    
    displayManager->updateMeasurementTime();
    displayManager->turnDisplayOn();
  }
  
  delay(50);  // Small delay for responsiveness
}