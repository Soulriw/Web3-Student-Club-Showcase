#include <Arduino.h>
#include <M5Core2.h>
#include <WiFi.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../include/ShowcaseProtocol.h"

// ============================================
// CONFIGURATION
// ============================================
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

// ============================================
// GLOBAL STATE
// ============================================
struct {
    String currentUsername;
    bool authInProgress;
    bool authSuccess;
    uint32_t lastUpdateTime;
} g_monitor = {
    "Waiting...",
    false,
    false,
    0
};

// ============================================
// UI FUNCTIONS
// ============================================
void drawUI() {
    M5.Lcd.fillScreen(TFT_BLACK);

    // ===== HEADER =====
    M5.Lcd.fillRect(0, 0, DISPLAY_WIDTH, 50, TFT_NAVY);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(3);
    M5.Lcd.drawString("STATION 2", 20, 10);

    // ===== STATUS AREA =====
    M5.Lcd.setTextColor(TFT_CYAN);
    M5.Lcd.setTextSize(2);
    M5.Lcd.drawString("Authentication", 20, 70);

    // ===== USERNAME DISPLAY =====
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.drawString("User: " + g_monitor.currentUsername, 20, 100);

    // ===== STATUS MESSAGE =====
    M5.Lcd.setTextSize(2);
    if (g_monitor.authInProgress) {
        M5.Lcd.setTextColor(TFT_ORANGE);
        M5.Lcd.drawString("Status: In Progress...", 20, 140);
    } else if (g_monitor.authSuccess) {
        M5.Lcd.setTextColor(TFT_GREEN);
        M5.Lcd.drawString("Status: ✓ Success!", 20, 140);
    } else {
        M5.Lcd.setTextColor(TFT_LIGHTGREY);
        M5.Lcd.drawString("Status: Ready", 20, 140);
    }

    // ===== INSTRUCTIONS =====
    M5.Lcd.setTextColor(TFT_LIGHTGREY);
    M5.Lcd.setTextSize(1);
    M5.Lcd.drawString("Waiting for authentication from Matrix...", 20, 200);
}

// ============================================
// ESP-NOW MESSAGE HANDLER
// ============================================
void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len != sizeof(ShowcaseMessage)) return;

    ShowcaseMessage msg;
    memcpy(&msg, incomingData, sizeof(msg));

    if (!verifyChecksum(msg)) {
        Serial.println("[WARN] Checksum verification failed");
        return;
    }

    switch (msg.type) {
        case MSG_IDENTITY_ASSIGN:
            if (msg.username[0] != '\0') {
                g_monitor.currentUsername = String(msg.username);
                g_monitor.authSuccess = false;
                g_monitor.authInProgress = false;
                Serial.printf("[OK] Identity received: %s\n", msg.username);
                drawUI();
            }
            break;

        case MSG_AUTH_REQUEST:
            g_monitor.authInProgress = true;
            Serial.println("[INFO] Auth in progress");
            drawUI();
            break;

        case MSG_AUTH_SUCCESS:
            g_monitor.authInProgress = false;
            g_monitor.authSuccess = true;
            Serial.println("[OK] Authentication successful");
            drawUI();
            break;

        case MSG_RESET_ALL:
            g_monitor.currentUsername = "Waiting...";
            g_monitor.authInProgress = false;
            g_monitor.authSuccess = false;
            Serial.println("[OK] System reset");
            drawUI();
            break;

        default:
            break;
    }
}

// ============================================
// SETUP
// ============================================
void setup() {
    M5.begin(true, true, true, true);
    Serial.begin(115200);
    delay(500);

    Serial.println("\n\n=== STATION 2: CORE2 MONITOR STARTING ===");

    // Draw initial UI
    drawUI();

    // Initialize WiFi
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);
    delay(100);

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR] ESP-NOW initialization failed");
        M5.Lcd.fillScreen(TFT_RED);
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.drawString("ESP-NOW Error!", 50, 100);
        while (1) delay(1000);
    }

    // Register broadcast peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
    peerInfo.channel = BROADCAST_CHANNEL;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[ERROR] Failed to add peer");
        return;
    }

    // Register receive callback
    esp_now_register_recv_cb(onDataRecv);

    Serial.println("[OK] ESP-NOW initialized");
    Serial.println("=== STATION 2 CORE2 MONITOR READY ===\n");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
    M5.update();

    // Periodic UI refresh
    if (millis() - g_monitor.lastUpdateTime > 2000) {
        drawUI();
        g_monitor.lastUpdateTime = millis();
    }

    delay(50);
}