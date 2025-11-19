#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <string>

//========================= LD2450 SENSOR PINS =========================
// UART2 for LD2450 Sensor
static constexpr int LD2450_TX_PIN = 9;              // GPIO9
static constexpr int LD2450_RX_PIN = 8;              // GPIO8
static constexpr int LD2450_BAUD_RATE = 256000;      // 256000 baud

// UART1 for Meshtastic (defined in main.cpp)
// TX=GPIO43, RX=GPIO44, Baudrate=115200
//=======================================================================

// JSON Output Parameter
static constexpr int PAYLOAD_OUTPUT_INTERVAL = 2000;  // Interval in milliseconds

// Gateway/Device Identification
extern std::string GATEWAY_ID;

#endif // CONFIG_H