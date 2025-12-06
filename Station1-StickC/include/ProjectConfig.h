#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#include <IPAddress.h>

// --- WiFi Settings ---
// Core2 จะเป็นคนสร้างวงนี้ขึ้นมา
const char* WIFI_SSID = "Web3_Showcase";
const char* WIFI_PASS = "12345678";

// --- Static IP Map (Fixed for Stability) ---
// Gateway (Core2) - Station 1 Host
const IPAddress IP_CORE2(192, 168, 4, 1);

// Wearable (User) - Station 1 Client
const IPAddress IP_STICKC(192, 168, 4, 2);

// Station 2 (Auth)
const IPAddress IP_MATRIX_ST2(192, 168, 4, 3);

// Station 3 (Earn)
const IPAddress IP_PAPER_ST3(192, 168, 4, 5);

// Station 4 (Spend)
const IPAddress IP_PAPER_ST4(192, 168, 4, 6);   // Menu
const IPAddress IP_MATRIX_ST4(192, 168, 4, 7);  // Payment Terminal

// Common & Admin
const IPAddress IP_ECHO(192, 168, 4, 4);        // Sound Server
const IPAddress IP_RESET(192, 168, 4, 8);       // Reset Button

// Ports
const int HTTP_PORT = 80;

// BLE Config
const char* BLE_DEVICE_NAME = "M5_Showcase_User";

#endif