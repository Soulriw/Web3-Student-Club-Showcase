#include <M5Unified.h>
#include <esp_now.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../include/ShowcaseProtocol.h"

// ============================================
// CONFIGURATION
// ============================================
#define LED_READY_COLOR 0x0000FF    // Blue
#define LED_PROCESSING_COLOR 0xFFFF00  // Yellow
#define LED_SUCCESS_COLOR 0x00FF00  // Green
#define LED_ERROR_COLOR 0xFF0000    // Red
#define LED_OFF 0x000000

#define TOUCH_THRESHOLD -2000  // Capacitive touch threshold

// ============================================
// GLOBAL STATE
// ============================================
struct {
    bool orderPending;
    bool orderConfirmed;
    bool userNearby;
    uint32_t lastOrderTime;
    uint32_t lastTouchTime;
} g_matrix4 = {
    false, false, false, 0, 0
};

// ============================================
// LED CONTROL
// ============================================
void setMatrixColor(uint32_t color) {
    // M5Unified uses drawPixel for LED matrix on ATOM devices
    for (int i = 0; i < 25; i++) {
        M5.Lcd.drawPixel(i % 5, i / 5, color);
    }
}

void pulseMatrix(uint32_t color, int pulses) {
    for (int p = 0; p < pulses; p++) {
        setMatrixColor(color);
        delay(200);
        setMatrixColor(LED_OFF);
        delay(200);
    }
    setMatrixColor(color);
}

// ============================================
// ESP-NOW COMMUNICATION
// ============================================
void sendOrderConfirmationToPaper() {
    if (!g_matrix4.orderPending) return;

    ShowcaseMessage msg = createMessage(MSG_SPEND_CONFIRM, "", 0, "");
    msg.status = 1;  // Success

    esp_err_t result = esp_now_send(BROADCAST_MAC, (uint8_t *)&msg, sizeof(msg));

    if (result == ESP_OK) {
        Serial.println("[OK] Order confirmation sent");
        g_matrix4.orderConfirmed = true;
        pulseMatrix(LED_SUCCESS_COLOR, 3);
    } else {
        Serial.println("[ERROR] Failed to send confirmation");
        pulseMatrix(LED_ERROR_COLOR, 2);
    }

    g_matrix4.orderPending = false;
    g_matrix4.lastOrderTime = millis();
}

void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len != sizeof(ShowcaseMessage)) return;

    ShowcaseMessage msg;
    memcpy(&msg, incomingData, sizeof(msg));

    if (!verifyChecksum(msg)) {
        Serial.println("[WARN] Checksum verification failed");
        return;
    }

    switch (msg.type) {
        case MSG_SPEND_REQUEST:
            Serial.printf("[INFO] Order received: %s (-%" PRId32 ")\n", msg.description, msg.amount);
            g_matrix4.orderPending = true;
            setMatrixColor(LED_PROCESSING_COLOR);
            // Wait for user tap
            break;

        case MSG_RESET_ALL:
            g_matrix4.orderPending = false;
            g_matrix4.orderConfirmed = false;
            setMatrixColor(LED_OFF);
            Serial.println("[OK] System reset");
            break;

        default:
            break;
    }
}

// ============================================
// FREERTOS TASKS
// ============================================

void taskProximityMonitor(void *parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(500);

    while (1) {
        // Proximity detection via capacitive touch
        // If user is nearby, show blue ready state
        if (!g_matrix4.orderPending && !g_matrix4.orderConfirmed) {
            setMatrixColor(LED_READY_COLOR);
        }

        vTaskDelayUntil(&lastWakeTime, interval);
    }
}

void taskTimeoutHandler(void *parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(1000);

    while (1) {
        // Reset confirmed state after 5 seconds
        if (g_matrix4.orderConfirmed &&
            (millis() - g_matrix4.lastOrderTime) > 5000) {
            g_matrix4.orderConfirmed = false;
            setMatrixColor(LED_READY_COLOR);
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

    Serial.println("\n\n=== STATION 4: ATOM MATRIX STARTING ===");

    // Initialize LED
    setMatrixColor(LED_READY_COLOR);

    // Initialize WiFi
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);
    delay(100);

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR] ESP-NOW initialization failed");
        setMatrixColor(LED_ERROR_COLOR);
        while (1) delay(1000);
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
    peerInfo.channel = BROADCAST_CHANNEL;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    esp_now_register_recv_cb(onDataRecv);

    Serial.println("[OK] ESP-NOW initialized");

    // Create FreeRTOS tasks
    xTaskCreatePinnedToCore(
        taskProximityMonitor,
        "ProximityTask",
        2048,
        NULL,
        1,
        NULL,
        0
    );

    xTaskCreatePinnedToCore(
        taskTimeoutHandler,
        "TimeoutTask",
        2048,
        NULL,
        1,
        NULL,
        1
    );

    Serial.println("[OK] FreeRTOS tasks created");
    Serial.println("=== STATION 4 MATRIX READY ===\n");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
    M5.update();

    // Button press - confirm order
    if (M5.BtnA.wasPressed()) {
        if (g_matrix4.orderPending) {
            Serial.println("[INFO] Button pressed - confirming order");
            sendOrderConfirmationToPaper();
        }
    }

    delay(100);
}