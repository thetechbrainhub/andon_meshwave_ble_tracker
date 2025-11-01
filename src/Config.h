#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <string>
#include <set>

//========================= KONFIGURIERBARE VARIABLEN =========================
// BLE-Scan Parameter
static constexpr int SCAN_TIME = 5;                // Scan-Dauer in Sekunden
static constexpr int SCAN_INTERVAL = 100;          // Scan-Intervall (in 0.625ms Einheiten)
static constexpr int SCAN_WINDOW = 99;             // Scan-Fenster (in 0.625ms Einheiten)
static constexpr bool ACTIVE_SCAN = true;          // Aktiver Scan verbraucht mehr Strom, liefert aber mehr Daten

// RSSI zu Meter Konvertierungsparameter
static constexpr int TX_POWER = -59;               // Kalibrierte Sendeleistung bei 1 Meter (je nach Beacon anpassen)
static constexpr float ENVIRONMENTAL_FACTOR = 2.7; // Pfadverlustexponent (2.0 für Freiraum, höher für Innenräume mit Hindernissen), 2.7 für Innenraum
static constexpr float DISTANCE_THRESHOLD = 1.0;   // Nur Geräte innerhalb dieser Entfernung anzeigen (Meter)
static constexpr float DISTANCE_CORRECTION = -0.5;  // Korrekturwert zur Anpassung der berechneten Distanz (in Metern), je niedriger desto näher rückt der Beacon

// Kalman-Filter Parameter
static constexpr float PROCESS_NOISE = 0.01;       // Prozessrauschen - höhere Werte folgen Änderungen schneller
static constexpr float MEASUREMENT_NOISE = 0.5;    // Messrauschen - höhere Werte glätten stärker

// Gleitender Mittelwert Parameter
static constexpr int WINDOW_SIZE = 5;              // Anzahl der Werte für den gleitenden Mittelwert

// Beacon Tracking Parameter
static constexpr int BEACON_TIMEOUT_SECONDS = 10;  // Timeout in Sekunden für Beacon-Tracking

// Gateway Identification
static const String GATEWAY_ID = "TRAC 001";         // Unique identifier for this gateway - change for multiple gateways

// Gerätefilter - Liste der MAC-Adressen, die überwacht werden sollen
// Leere String = Alle Geräte überwachen, ansonsten komma-getrennte Liste, z.B. "e0:80:8f:1e:13:28,e4:b3:23:c1:f6:2a"
// WHOOP e0:80:8f:1e:13:28, NGIS 004 08:05:04:03:02:01, BLE MPU Test e4:b0:63:41:7d:5a
static const String DEVICE_FILTER = "08:05:04:03:02:01,0d:03:0a:02:0e:01,e4:b0:63:41:7d:5a";  // Hier die gewünschten MAC-Adressen eintragen um mit Komma ohne Leerschritt trennen
static constexpr bool USE_DEVICE_FILTER = true;  // Auf true setzen, um Filter zu aktivieren

// JSON-Ausgabe Parameter
static constexpr int JSON_OUTPUT_INTERVAL = 2000;  // Intervall für JSON-Ausgabe in Millisekunden

// UART Parameter für Meshtastic
static constexpr int UART_TX_PIN = 43;             // GPIO-Pin für UART TX
static constexpr int UART_RX_PIN = 44;             // GPIO-Pin für UART RX
static constexpr int UART_BAUD_RATE = 115200;      // Baudrate für UART
//=============================================================================

// Globale Variablen, die in mehreren Dateien verwendet werden
extern std::set<std::string> filteredDevices;
extern int devicesInRangeCount;

// Prototyp für die Funktion, die in mehreren Dateien verwendet wird
float rssiToMeters(int rssi);
bool isDeviceInFilter(const std::string& address);
void parseDeviceFilter();

#endif // CONFIG_H