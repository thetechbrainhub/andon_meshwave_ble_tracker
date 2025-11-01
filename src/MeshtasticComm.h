#ifndef MESHTASTICCOMM_H
#define MESHTASTICCOMM_H

#include <Arduino.h>
#include <string>
#include "DeviceInfo.h"

// Global serial interface
extern HardwareSerial MeshtasticSerial;

// ✅ Message buffer (character-based)
extern std::string receivedChars;
extern unsigned long lastCharTime;

/**
 * Initialize Meshtastic UART communication
 * ✅ WICHTIG: .begin() wird NICHT hier aufgerufen!
 * Das passiert jetzt in main.cpp setup()
 * Call this in setup() NACH MeshtasticSerial.begin()
 */
void initMeshtasticComm();

/**
 * Send beacon data to Meshtastic
 * @param address MAC address of beacon
 * @param device Device info
 * @param lastSeenOverride Override for last seen time
 */
void sendBeaconToMeshtastic(const std::string& address, DeviceInfo& device, float lastSeenOverride);

/**
 * Check for incoming Meshtastic commands
 * Character-based buffering with timeout processing
 * Call this every loop iteration
 */
void checkForMeshtasticCommands();

/**
 * Process complete JSON message from buffer
 * Detects config commands and calls ConfigManager
 * Called internally by checkForMeshtasticCommands()
 */
void processReceivedJSON();

#endif // MESHTASTICCOMM_H