#include <M5StickCPlus2.h>
#include <esp_now.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "ShowcaseProtocol.h"

// ============================================
// CONFIGURATION
// ============================================
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 135
#define BATTERY_UPDATE_INTERVAL 1000  // 1 second
#define UI_UPDATE_INTERVAL 500        // 500ms for smooth rendering

// ============================================
// GLOBAL STATE
// ============================================
struct {
    String username;
    int32_t balance;
    bool authenticated;
    String status;
    float batteryVoltage;
    int batteryPercent;
    uint32_t lastMessageTime;
} g_state = {
    "Guest",
    0,
    false,
    "Waiting...",
    4.2f,
    100,
    0
};

// ============================================
// AUDIO FUNCTIONS (Non-blocking)
// ============================================
void playToneNonBlocking(int freq, int duration) {
    M5.Speaker.tone(freq, duration);
}

void playSuccessTone() {
    playToneNonBlocking(1000, 50);
    delay(50);
    playToneNonBlocking(2000, 50);
    delay(50);
    playToneNonBlocking(1500, 100);
}

void playErrorTone() {
    playToneNonBlocking(500, 200);
    delay(100);
    playToneNonBlocking(300, 200);
}

void playAuthRequestTone() {
    playToneNonBlocking(1200, 100);
    delay(100);
    playToneNonBlocking(1200, 100);
}

// ============================================
// BATTERY MONITORING
// ============================================
void updateBatteryStatus() {
    // Get battery info from M5StickCPlus2
    g_state.batteryVoltage = M5.Power.getBatteryVoltage() / 1000.0f;
    g_state.batteryPercent = M5.Power.getBatteryLevel();
    
    // Clamp to safe values
    if (g_state.batteryPercent < 0) g_state.batteryPercent = 0;
    if (g_state.batteryPercent > 100) g_state.batteryPercent = 100;
}

uint16_t getBatteryColor() {
    if (g_state.batteryPercent > 70) return TFT_GREEN;
    // TFT_ORANGE may not be defined in all cores/libs; use a literal 16-bit RGB565 orange color
    if (g_state.batteryPercent > 40) return 0xFD20; // approx orange
    return TFT_RED;
}

void drawBatteryBar(int x, int y, int width, int height) {
    uint16_t color = getBatteryColor();
    
    // Background
    M5.Lcd.drawRect(x, y, width, height, TFT_WHITE);
    
    // Filled portion
    int filledWidth = (g_state.batteryPercent * width) / 100;
    if (filledWidth > 0) {
        M5.Lcd.fillRect(x + 1, y + 1, filledWidth - 1, height - 2, color);
    }
    
    // Percentage text
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(1);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", g_state.batteryPercent);
    M5.Lcd.drawString(buf, x + width + 5, y);
}

// ============================================
// UI DRAWING (Main update function)
// ============================================
void drawUI() {
    M5.Lcd.fillScreen(TFT_BLACK);
    
    // ===== HEADER =====
    M5.Lcd.fillRect(0, 0, SCREEN_WIDTH, 25, TFT_NAVY);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.drawString("STATION 1", 5, 3);
    
    // Auth indicator (right side)
    M5.Lcd.setTextColor(g_state.authenticated ? TFT_GREEN : TFT_RED);
    M5.Lcd.setTextSize(2);
    M5.Lcd.drawString(g_state.authenticated ? "✓" : "✗", SCREEN_WIDTH - 25, 3);
    
    // ===== USERNAME SECTION =====
    M5.Lcd.setTextColor(TFT_CYAN);
    M5.Lcd.setTextSize(1);
    M5.Lcd.drawString("User:", 5, 30);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(1);
    String displayName = g_state.username;
    if (displayName.length() > 14) displayName = displayName.substring(0, 14) + "..";
    M5.Lcd.drawString(displayName, 5, 40);
    
    // ===== BALANCE SECTION =====
    M5.Lcd.setTextColor(TFT_YELLOW);
    M5.Lcd.setTextSize(1);
    M5.Lcd.drawString("Balance:", 5, 52);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(2);
    char balBuf[32];
    snprintf(balBuf, sizeof(balBuf), "%d coins", g_state.balance);
    M5.Lcd.drawString(balBuf, 5, 62);
    
    // ===== STATUS SECTION =====
    M5.Lcd.setTextColor(TFT_LIGHTGREY);
    M5.Lcd.setTextSize(1);
    M5.Lcd.drawString("Status:", 5, 85);
    String statusDisplay = g_state.status;
    if (statusDisplay.length() > 25) statusDisplay = statusDisplay.substring(0, 22) + "...";
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.drawString(statusDisplay, 5, 95);
    
    // ===== BATTERY SECTION =====
    M5.Lcd.setTextColor(TFT_LIGHTGREY);
    M5.Lcd.setTextSize(1);
    M5.Lcd.drawString("Battery:", 5, 110);
    drawBatteryBar(5, 118, 80, 10);
    
    // Voltage display
    char voltBuf[12];
    snprintf(voltBuf, sizeof(voltBuf), "%.2fv", g_state.batteryVoltage);
    M5.Lcd.setTextColor(TFT_LIGHTGREY);
    M5.Lcd.setTextSize(1);
    M5.Lcd.drawString(voltBuf, 90, 120);
}

// ============================================
// MESSAGE HANDLER - ESP-NOW Callback
// ============================================
void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len != sizeof(ShowcaseMessage)) {
        Serial.printf("[WARN] Invalid message size: %d\n", len);
        return;
    }

    ShowcaseMessage msg;
    memcpy(&msg, incomingData, sizeof(msg));

    // Verify checksum
    if (!verifyChecksum(msg)) {
        Serial.println("[WARN] Checksum verification failed");
        return;
    }

    g_state.lastMessageTime = millis();
    bool shouldPlaySound = true;
    bool shouldRedraw = true;

    switch (msg.type) {
        case MSG_IDENTITY_ASSIGN:
            if (msg.username[0] != '\0') {
                g_state.username = String(msg.username);
                g_state.balance = 0;
                g_state.authenticated = false;
                g_state.status = "Identity registered";
                playSuccessTone();
                Serial.printf("[OK] Identity assigned: %s\n", msg.username);
            }
            break;

        case MSG_AUTH_REQUEST:
            g_state.status = "Auth in progress...";
            playAuthRequestTone();
            Serial.println("[INFO] Authentication in progress");
            break;

        case MSG_AUTH_SUCCESS:
            g_state.authenticated = true;
            g_state.status = "✓ Authenticated!";
            playSuccessTone();
            Serial.println("[OK] Authentication successful");
            break;

        case MSG_EARN_COIN:
            if (!g_state.authenticated) {
                g_state.status = "Auth required first";
                playErrorTone();
                Serial.println("[WARN] Earn attempt without auth");
            } else {
                g_state.balance += msg.amount;
                if (msg.description[0] != '\0') {
                    g_state.status = "+" + String(msg.amount) + " (" + String(msg.description) + ")";
                } else {
                    g_state.status = "+" + String(msg.amount) + " coins earned";
                }
                playSuccessTone();
                Serial.printf("[OK] Earned %d coins\n", msg.amount);
            }
            break;

        case MSG_SPEND_CONFIRM:
            if (!g_state.authenticated) {
                g_state.status = "Auth required";
                playErrorTone();
            } else if (g_state.balance < msg.amount) {
                g_state.status = "Insufficient balance!";
                playErrorTone();
                Serial.println("[WARN] Insufficient balance");
            } else {
                g_state.balance -= msg.amount;
                if (msg.description[0] != '\0') {
                    g_state.status = "-" + String(msg.amount) + " (" + String(msg.description) + ")";
                } else {
                    g_state.status = "-" + String(msg.amount) + " coins spent";
                }
                playSuccessTone();
                Serial.printf("[OK] Spent %d coins\n", msg.amount);
            }
            break;

        case MSG_RESET_ALL:
            g_state.username = "Guest";
            g_state.balance = 0;
            g_state.authenticated = false;
            g_state.status = "System reset";
            playToneNonBlocking(500, 500);
            Serial.println("[OK] System reset received");
            break;

        default:
            shouldRedraw = false;
            shouldPlaySound = false;
            Serial.printf("[WARN] Unknown message type: %d\n", msg.type);
            break;
    }

    if (shouldRedraw) {
        drawUI();
    }
}

// ============================================
// FREERTOS TASKS
// ============================================

// Task: Battery monitor - updates every 1 second
void taskBatteryMonitor(void *parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(BATTERY_UPDATE_INTERVAL);

    while (1) {
        updateBatteryStatus();
        vTaskDelayUntil(&lastWakeTime, interval);
    }
}

// Task: UI updater - refreshes display every 500ms
void taskUIUpdate(void *parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(UI_UPDATE_INTERVAL);

    while (1) {
        drawUI();
        vTaskDelayUntil(&lastWakeTime, interval);
    }
}

// ============================================
// SETUP
// ============================================
void setup() {
    M5.begin();
    M5.Lcd.setRotation(3);  // Landscape mode
    Serial.begin(115200);
    delay(500);

    Serial.println("\n\n=== STATION 1: STICKC-PLUS2 STARTING ===");

    // Initialize display
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.drawString("Initializing...", 10, 60);

    // Initialize WiFi (for ESP-NOW compatibility)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);
    delay(100);

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR] ESP-NOW initialization failed");
        M5.Lcd.fillScreen(TFT_RED);
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.drawString("ESP-NOW Error!", 50, 60);
        while (1) delay(1000);
    }
    Serial.println("[OK] ESP-NOW initialized");

    // Register receive callback
    esp_now_register_recv_cb(onDataRecv);

    // Get initial battery status
    updateBatteryStatus();

    // Draw initial UI
    drawUI();

    // Create FreeRTOS tasks
    xTaskCreatePinnedToCore(
        taskBatteryMonitor,   // Function
        "BatteryTask",        // Name
        2048,                 // Stack size
        NULL,                 // Parameter
        1,                    // Priority
        NULL,                 // Task handle
        0                     // Core
    );

    xTaskCreatePinnedToCore(
        taskUIUpdate,         // Function
        "UITask",             // Name
        2048,                 // Stack size
        NULL,                 // Parameter
        1,                    // Priority
        NULL,                 // Task handle
        1                     // Core (different from battery task)
    );

    Serial.println("[OK] FreeRTOS tasks created");
    Serial.println("=== STATION 1 STICKC READY ===\n");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
    // Update M5 state (handles button presses, etc.)
    M5.update();

    // Check for timeout (no messages for 30 seconds)
    if (g_state.lastMessageTime > 0 && 
        (millis() - g_state.lastMessageTime) > 30000) {
        if (g_state.status != "No signal") {
            g_state.status = "No signal (timeout)";
            drawUI();
        }
    }

    // Sleep to reduce power consumption
    delay(100);
}
