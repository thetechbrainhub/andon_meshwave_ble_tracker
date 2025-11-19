#ifndef MESHTASTICCOMM_H
#define MESHTASTICCOMM_H

#include <Arduino.h>
#include <string>

// Global serial interface for Meshtastic UART1
extern HardwareSerial* MeshtasticSerial;

// Message buffer
extern std::string receivedChars;
extern unsigned long lastCharTime;

/**
 * Initialize Meshtastic UART communication
 * Call this in setup() after UART1 initialization
 */
void initMeshtasticComm();

/**
 * Send payload via Meshtastic
 * @param payload JSON string to send
 */
void sendPayloadViaMeshtastic(const String& payload);

/**
 * Check for incoming Meshtastic commands
 * Character-based buffering with timeout processing
 * Call this every loop iteration
 */
void checkForMeshtasticCommands();

/**
 * Process complete JSON message from buffer
 * Detects config commands and calls appropriate manager
 * Called internally by checkForMeshtasticCommands()
 */
void processReceivedJSON();

#endif // MESHTASTICCOMM_H