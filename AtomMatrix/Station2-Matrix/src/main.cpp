#include <M5Unified.h>
#include <esp_now.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../include/ShowcaseProtocol.h"

// ============================================
// CONFIGURATION
// ============================================
#define RSSI_THRESHOLD -50
#define RSSI_CHECK_INTERVAL 500
#define LED_BRIGHTNESS 50

// Pixel layout for 5x5 LED matrix
#define LED_READY_COLOR 0x0000FF    // Blue
#define LED_SUCCESS_COLOR 0x00FF00  // Green
#define LED_ERROR_COLOR 0xFF0000    // Red
#define LED_OFF 0x000000

// ============================================
// GLOBAL STATE
// ============================================
struct {
    bool targetDetected;
    bool authInProgress;
    bool authSuccess;
    uint32_t lastDetectTime;
    uint32_t lastStatusUpdate;
    String lastError;
    uint8_t echoMacAddress[6];  // Echo device MAC
} g_matrix = {
    false, false, false, 0, 0, "", {0}
};

// ============================================
// FORWARD DECLARATIONS
// ============================================
void sendAuthRequestToEcho();

// ============================================
// LED CONTROL FUNCTIONS
// ============================================
void setMatrixColor(uint32_t color) {
    for (int i = 0; i < 25; i++) {
        M5.Lcd.drawPixel(i % 5, i / 5, color);
    }
    M5.Lcd.display();
}

void showReadyState() {
    setMatrixColor(LED_READY_COLOR);
    Serial.println("[UI] Matrix: Ready (Blue)");
}

void showSuccessState() {
    setMatrixColor(LED_SUCCESS_COLOR);
    Serial.println("[UI] Matrix: Success (Green)");
}

void showErrorState() {
    setMatrixColor(LED_ERROR_COLOR);
    Serial.println("[UI] Matrix: Error (Red)");
}

void showOffState() {
    setMatrixColor(LED_OFF);
    Serial.println("[UI] Matrix: Off");
}

// Animated loading pattern
void showLoadingAnimation() {
    static uint8_t frame = 0;
    uint32_t colors[] = {0xFF0000, 0xFF7F00, 0xFFFF00, 0x00FF00, 0x0000FF, 0x8B00FF};
    
    M5.Lcd.fillScreen(TFT_BLACK);
    for (int i = 0; i < 25; i++) {
        M5.Lcd.drawPixel(i % 5, i / 5, colors[(i + frame) % 6]);
    }
    M5.Lcd.display();
    frame = (frame + 1) % 6;
}

// ============================================
// BLE/WiFi SCANNING FOR TARGET (Enhanced with friend's code)
// ============================================
void scanForTarget() {
    // Scan WiFi networks for Web3_StickC or Web3_Showcase
    int networksFound = WiFi.scanNetworks();
    
    g_matrix.targetDetected = false;
    int maxRSSI = -100;  // Track strongest signal
    
    if (networksFound > 0) {
        for (int i = 0; i < networksFound; i++) {
            int rssi = WiFi.RSSI(i);
            String ssid = WiFi.SSID(i);
            
            // Check if this is a Web3 device and signal is strong enough
            if (ssid.indexOf("Web3") >= 0) {
                if (rssi > RSSI_THRESHOLD && rssi > maxRSSI) {
                    maxRSSI = rssi;
                    g_matrix.targetDetected = true;
                    g_matrix.lastDetectTime = millis();
                    Serial.printf("[OK] Target detected: %s (RSSI: %d dBm)\n", ssid.c_str(), rssi);
                }
            }
        }
    } else {
        // Fallback: If no WiFi networks detected, try WiFi RSSI of known AP
        // This helps detect StickC even if it's not advertising its own AP
        int rssi = WiFi.scanNetworks(false, false, false, 200);
        if (rssi != WIFI_SCAN_RUNNING) {
            g_matrix.targetDetected = (rssi > RSSI_THRESHOLD);
        }
    }
    
    WiFi.scanDelete();
}

// ============================================
// ESP-NOW COMMUNICATION
// ============================================
void sendAuthRequestToEcho() {
    if (g_matrix.authInProgress) {
        Serial.println("[WARN] Auth already in progress");
        return;
    }

    g_matrix.authInProgress = true;
    showLoadingAnimation();
    
    ShowcaseMessage msg = createMessage(MSG_AUTH_REQUEST, "", 0, "");
    
    // Send via ESP-NOW broadcast
    esp_err_t result = esp_now_send(BROADCAST_MAC, (uint8_t *)&msg, sizeof(msg));
    
    if (result == ESP_OK) {
        Serial.println("[OK] Auth request sent to Echo");
    } else {
        Serial.println("[ERROR] Failed to send auth request");
        g_matrix.authInProgress = false;
        showErrorState();
    }
}

void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len != sizeof(ShowcaseMessage)) return;

    ShowcaseMessage msg;
    memcpy(&msg, incomingData, sizeof(msg));

    if (!verifyChecksum(msg)) {
        Serial.println("[WARN] Checksum failed");
        return;
    }

    switch (msg.type) {
        case MSG_AUTH_SUCCESS: {
            g_matrix.authInProgress = false;
            g_matrix.authSuccess = true;
            g_matrix.lastStatusUpdate = millis();
            showSuccessState();
            Serial.println("[OK] Authentication successful");
            // Keep success state for 3 seconds
            vTaskDelay(pdMS_TO_TICKS(3000));
            break;
        }

        case MSG_RESET_ALL: {
            g_matrix.authSuccess = false;
            g_matrix.authInProgress = false;
            showOffState();
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;
        }

        default:
            break;
    }
}

// ============================================
// FREERTOS TASKS
// ============================================

// Task: Scan for target device
void taskScanTarget(void *parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(RSSI_CHECK_INTERVAL);

    while (1) {
        scanForTarget();
        vTaskDelayUntil(&lastWakeTime, interval);
    }
}

// Task: Main logic handler
void taskMainLogic(void *parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(100);

    while (1) {
        // Handle state machine
        if (g_matrix.authSuccess) {
            // Already completed
            vTaskDelayUntil(&lastWakeTime, interval);
            continue;
        }

        if (g_matrix.authInProgress) {
            // Show loading animation
            showLoadingAnimation();
            vTaskDelayUntil(&lastWakeTime, interval);
            continue;
        }

        if (g_matrix.targetDetected) {
            // Target in range, show ready
            if (!g_matrix.authInProgress) {
                showReadyState();
                // Wait for button press or timeout
                if (millis() - g_matrix.lastDetectTime > 5000) {
                    // Timeout - auto-trigger
                    sendAuthRequestToEcho();
                }
            }
        } else {
            // No target - show blue waiting
            showReadyState();
        }

        vTaskDelayUntil(&lastWakeTime, interval);
    }
}

// ============================================
// SETUP
// ============================================
void setup() {
    M5.begin();
    Serial.begin(115200);
    delay(500);

    Serial.println("\n\n=== STATION 2: ATOM MATRIX STARTING ===");

    // Initialize display
    setMatrixColor(0x0000FF);
    Serial.println("[OK] LED matrix initialized");

    // Initialize WiFi for scanning
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);
    delay(100);

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR] ESP-NOW init failed");
        setMatrixColor(LED_ERROR_COLOR);
        while (1) delay(1000);
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
    // Ensure channel matches Atom_Echo which forces channel 1
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    esp_now_register_recv_cb(onDataRecv);
    Serial.println("[OK] ESP-NOW initialized");

    // Create FreeRTOS tasks
    xTaskCreatePinnedToCore(
        taskScanTarget,
        "ScanTask",
        2048,
        NULL,
        1,
        NULL,
        0
    );

    xTaskCreatePinnedToCore(
        taskMainLogic,
        "LogicTask",
        2048,
        NULL,
        2,
        NULL,
        1
    );

    Serial.println("[OK] FreeRTOS tasks created");
    Serial.println("=== STATION 2 MATRIX READY ===\n");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
    M5.update();

    // Button A pressed
    if (M5.BtnA.wasPressed()) {
        if (g_matrix.targetDetected && !g_matrix.authInProgress) {
            Serial.println("[INFO] Button pressed - triggering auth");
            sendAuthRequestToEcho();
        }
    }

    delay(50);
}
