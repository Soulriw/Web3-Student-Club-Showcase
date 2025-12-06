#include <Arduino.h>
#include <M5Unified.h>
#include <esp_now.h>
#include <WiFi.h>
#include "../include/ShowcaseProtocol.h"

// ============================================
// CONFIGURATION
// ============================================
#define LED_IDLE_COLOR 0x00FF00      // Green - idle
#define LED_RESET_COLOR 0xFF0000     // Red - resetting
#define LED_OFF 0x000000

#define RESET_HOLD_TIME 1000  // 1 second hold to trigger reset

// ============================================
// GLOBAL STATE
// ============================================
struct {
    uint32_t buttonPressTime;
    bool resetInProgress;
    uint32_t resetStartTime;
} g_reset = {
    0, false, 0
};

// ============================================
// LED FUNCTIONS
// ============================================
void setMatrixColor(uint32_t color) {
    // M5Unified supports drawPixel for LED matrix
    for (int i = 0; i < 25; i++) {
        M5.Lcd.drawPixel(i % 5, i / 5, color);
    }
}

void blinkMatrix(uint32_t color, int count) {
    for (int i = 0; i < count; i++) {
        setMatrixColor(color);
        delay(200);
        setMatrixColor(LED_OFF);
        delay(200);
    }
}

// ============================================
// RESET BROADCAST
// ============================================
void broadcastReset() {
    if (g_reset.resetInProgress) return;

    g_reset.resetInProgress = true;
    g_reset.resetStartTime = millis();

    Serial.println("\n[!!!] SYSTEM RESET TRIGGERED [!!!]");
    setMatrixColor(LED_RESET_COLOR);

    // Create reset message
    ShowcaseMessage resetMsg = createMessage(MSG_RESET_ALL, "", 0, "");

    // Send reset broadcast 3 times for redundancy
    for (int attempt = 0; attempt < 3; attempt++) {
        esp_err_t result = esp_now_send(BROADCAST_MAC, (uint8_t *)&resetMsg, sizeof(resetMsg));
        if (result == ESP_OK) {
            Serial.printf("[OK] Reset broadcast #%d sent\n", attempt + 1);
        } else {
            Serial.printf("[WARN] Reset broadcast #%d failed\n", attempt + 1);
        }
        delay(100);
    }

    // Blink red to confirm
    blinkMatrix(LED_RESET_COLOR, 5);

    // Return to idle state
    setMatrixColor(LED_IDLE_COLOR);
    g_reset.resetInProgress = false;

    Serial.println("[OK] System reset complete\n");
}

// ============================================
// ESP-NOW INITIALIZATION
// ============================================
void initESPNow() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);
    delay(100);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR] ESP-NOW initialization failed");
        setMatrixColor(0xFF0000);
        while (1) delay(1000);
    }

    // Add broadcast peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
    peerInfo.channel = BROADCAST_CHANNEL;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[ERROR] Failed to add peer");
        return;
    }

    Serial.println("[OK] ESP-NOW initialized");
}

// ============================================
// SETUP
// ============================================
void setup() {
    M5.begin();
    Serial.begin(115200);
    delay(500);

    Serial.println("\n\n=== RESET SYSTEM: ATOM MATRIX STARTING ===");

    // Initialize LED
    setMatrixColor(LED_IDLE_COLOR);
    Serial.println("[OK] LED matrix initialized (Green = Ready)");

    // Initialize ESP-NOW
    initESPNow();

    Serial.println("=== RESET SYSTEM READY ===");
    Serial.println("Hold Button A for 1 second to reset all stations\n");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
    M5.update();

    // Check button state
    if (M5.BtnA.isPressed()) {
        // Button is being held
        if (g_reset.buttonPressTime == 0) {
            // First frame of press
            g_reset.buttonPressTime = millis();
            Serial.println("[INFO] Button pressed - hold for reset");
        }

        // Check if held long enough
        uint32_t holdDuration = millis() - g_reset.buttonPressTime;
        if (holdDuration >= RESET_HOLD_TIME && !g_reset.resetInProgress) {
            broadcastReset();
            g_reset.buttonPressTime = 0;  // Reset counter
        }
    } else {
        // Button released
        if (g_reset.buttonPressTime > 0) {
            uint32_t pressDuration = millis() - g_reset.buttonPressTime;
            if (pressDuration < RESET_HOLD_TIME) {
                Serial.printf("[DEBUG] Button press too short (%lu ms)\n", pressDuration);
            }
            g_reset.buttonPressTime = 0;
        }
    }

    delay(50);
}