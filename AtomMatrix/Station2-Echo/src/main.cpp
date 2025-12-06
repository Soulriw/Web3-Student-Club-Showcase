#include <Arduino.h>
#include <M5Unified.h>
#include <esp_now.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../include/ShowcaseProtocol.h"

// ============================================
// CONFIGURATION
// ============================================
#define BEEP_FREQ_LOW 800
#define BEEP_FREQ_MID 1200
#define BEEP_FREQ_HIGH 1600
#define BEEP_DURATION_SHORT 100
#define BEEP_DURATION_MED 300
#define BEEP_DURATION_LONG 1000

// ============================================
// GLOBAL STATE
// ============================================
struct {
    bool authRequested;
    bool authSuccess;
    uint32_t lastMessageTime;
} g_echo = {
    false, false, 0
};

// ============================================
// AUDIO FUNCTIONS
// ============================================
void beepShort() {
    M5.Speaker.tone(BEEP_FREQ_MID, BEEP_DURATION_SHORT);
    delay(BEEP_DURATION_SHORT + 50);
}

void beepMedium() {
    M5.Speaker.tone(BEEP_FREQ_MID, BEEP_DURATION_MED);
    delay(BEEP_DURATION_MED + 50);
}

void beepLong() {
    M5.Speaker.tone(BEEP_FREQ_HIGH, BEEP_DURATION_LONG);
    delay(BEEP_DURATION_LONG + 50);
}

void playAuthSequence() {
    // Pattern: beep -> wait 1s -> beep -> wait 1s -> beep long
    beepShort();
    delay(1000);
    beepShort();
    delay(1000);
    beepLong();
}

void playErrorBeep() {
    M5.Speaker.tone(BEEP_FREQ_LOW, 500);
    delay(500);
}

void playSuccessBeep() {
    beepShort();
    delay(100);
    beepShort();
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

    g_echo.lastMessageTime = millis();

    switch (msg.type) {
        case MSG_AUTH_REQUEST: {
            Serial.println("[INFO] Authentication request received");
            g_echo.authRequested = true;
            playAuthSequence();
            
            // Send confirmation back
            ShowcaseMessage response = createMessage(MSG_AUTH_SUCCESS, "", 0, "");
            esp_err_t result = esp_now_send(BROADCAST_MAC, (uint8_t *)&response, sizeof(response));
            if (result == ESP_OK) {
                Serial.println("[OK] Auth success confirmation sent");
            }
            break;
        }

        case MSG_RESET_ALL: {
            Serial.println("[INFO] System reset received");
            g_echo.authRequested = false;
            g_echo.authSuccess = false;
            break;
        }

        default:
            break;
    }
}

// ============================================
// SETUP
// ============================================
void setup() {
    M5.begin();
    Serial.begin(115200);
    delay(500);

    Serial.println("\n\n=== STATION 2: ATOM ECHO STARTING ===");

    // Initialize WiFi for ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);
    delay(100);

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR] ESP-NOW initialization failed");
        playErrorBeep();
        while (1) delay(1000);
    }

    // Register broadcast peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
    peerInfo.channel = BROADCAST_CHANNEL;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[ERROR] Failed to add peer");
        playErrorBeep();
        return;
    }

    // Register receive callback
    esp_now_register_recv_cb(onDataRecv);

    Serial.println("[OK] ESP-NOW initialized");
    Serial.println("[OK] Atom Echo ready - listening for authentication requests");
    Serial.println("=== STATION 2 ECHO READY ===\n");

    // Play startup tone
    beepShort();
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
    M5.update();

    // Check button (can be used for testing)
    if (M5.BtnA.wasPressed()) {
        Serial.println("[DEBUG] Button pressed - playing test sequence");
        playAuthSequence();
    }

    delay(100);
}