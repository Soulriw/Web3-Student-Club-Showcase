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
#define PAPER_WIDTH 960
#define PAPER_HEIGHT 540
#define MENU_COUNT 6

// ============================================
// MENU DEFINITIONS (Food/Rewards)
// ============================================
struct MenuItem {
    const char *name;
    int32_t cost;
    const char *emoji;
};

MenuItem menuItems[MENU_COUNT] = {
    {"Iced Coffee", 80, "☕"},
    {"Pizza Slice", 120, "🍕"},
    {"Ice Cream", 60, "🍦"},
    {"Premium Gift", 200, "🎁"},
    {"Movie Ticket", 150, "🎬"},
    {"Snack Pack", 40, "🍿"}
};

// ============================================
// GLOBAL STATE
// ============================================
struct {
    String currentUsername;
    int32_t balance;
    int selectedMenuIdx;
    bool orderInProgress;
    String lastOrderStatus;
    uint32_t lastUpdateTime;
} g_paper4 = {
    "Guest",
    0,
    -1,
    false,
    "Ready",
    0
};

// ============================================
// FORWARD DECLARATIONS
// ============================================
void submitOrder();

// ============================================
// UI DRAWING
// ============================================
void drawUI() {
    M5.Lcd.fillScreen(TFT_WHITE);

    // ===== HEADER =====
    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextColor(TFT_BLACK);
    M5.Lcd.drawString("STATION 4: Spend Tokens", 50, 30);

    // ===== USER INFO =====
    char userBuf[128];
    snprintf(userBuf, sizeof(userBuf), "User: %s | Balance: %d coins",
             g_paper4.currentUsername.c_str(), g_paper4.balance);
    M5.Lcd.setTextSize(2);
    M5.Lcd.drawString(userBuf, 50, 90);

    // ===== MENU TITLE =====
    M5.Lcd.drawString("Select an item:", 50, 150);

    // ===== MENU GRID (3x2) =====
    int boxWidth = 280;
    int boxHeight = 100;
    int spacing = 20;
    int startX = 50;
    int startY = 200;

    for (int i = 0; i < MENU_COUNT; i++) {
        int col = i % 3;
        int row = i / 3;
        int xPos = startX + col * (boxWidth + spacing);
        int yPos = startY + row * (boxHeight + spacing);

        // Draw box with selection border
        uint16_t boxColor = (g_paper4.selectedMenuIdx == i) ? TFT_BLACK : TFT_LIGHTGREY;
        M5.Lcd.drawRect(xPos, yPos, boxWidth, boxHeight, boxColor);

        // Menu item text
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(TFT_BLACK);
        char menuBuf[64];
        snprintf(menuBuf, sizeof(menuBuf), "%s %s %d",
                 menuItems[i].emoji, menuItems[i].name, menuItems[i].cost);
        M5.Lcd.drawString(menuBuf, xPos + 10, yPos + 15);

        // Selected indicator
        if (g_paper4.selectedMenuIdx == i) {
            M5.Lcd.drawString("✓", xPos + boxWidth - 40, yPos + 10);
        }
    }

    // ===== ORDER BUTTON =====
    int orderY = 450;
    int orderWidth = 300;
    int orderHeight = 60;
    int orderX = (PAPER_WIDTH - orderWidth) / 2;

    M5.Lcd.drawRect(orderX, orderY, orderWidth, orderHeight, TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_BLACK);
    M5.Lcd.drawString("Order Now", orderX + 60, orderY + 12);

    // ===== STATUS =====
    M5.Lcd.setTextSize(1);
    M5.Lcd.drawString(g_paper4.lastOrderStatus.c_str(), 50, 510);
}

void drawProcessingScreen() {
    M5.Lcd.fillScreen(TFT_WHITE);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextColor(TFT_BLACK);
    M5.Lcd.drawString("Processing Order...", 200, 200);
}

void drawErrorScreen(const String &msg) {
    M5.Lcd.fillScreen(TFT_WHITE);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextColor(TFT_BLACK);
    M5.Lcd.drawString("Error:", 50, 200);
    M5.Lcd.setTextSize(2);
    M5.Lcd.drawString(msg.c_str(), 50, 280);
    M5.Lcd.drawString("Try again...", 50, 400);
    delay(2000);
}

// ============================================
// TOUCH INPUT HANDLER
// ============================================
void handleTouchInput() {
    auto t = M5.Touch.getDetail();
    if (!t.isPressed()) return;

    uint16_t touchX = t.x;
    uint16_t touchY = t.y;

    // Check menu selection
    int boxWidth = 280;
    int boxHeight = 100;
    int spacing = 20;
    int startX = 50;
    int startY = 200;

    for (int i = 0; i < MENU_COUNT; i++) {
        int col = i % 3;
        int row = i / 3;
        int xPos = startX + col * (boxWidth + spacing);
        int yPos = startY + row * (boxHeight + spacing);

        if (touchX >= xPos && touchX <= xPos + boxWidth &&
            touchY >= yPos && touchY <= yPos + boxHeight) {
            g_paper4.selectedMenuIdx = i;
            drawUI();
            return;
        }
    }

    // Check order button
    int orderY = 450;
    int orderWidth = 300;
    int orderHeight = 60;
    int orderX = (PAPER_WIDTH - orderWidth) / 2;

    if (touchX >= orderX && touchX <= orderX + orderWidth &&
        touchY >= orderY && touchY <= orderY + orderHeight) {
        if (g_paper4.selectedMenuIdx >= 0) {
            submitOrder();
        }
    }
}

void submitOrder() {
    if (g_paper4.selectedMenuIdx < 0 || g_paper4.orderInProgress) return;

    MenuItem &selected = menuItems[g_paper4.selectedMenuIdx];

    // Check balance
    if (g_paper4.balance < selected.cost) {
        drawErrorScreen("Insufficient Balance!");
        drawUI();
        return;
    }

    g_paper4.orderInProgress = true;
    drawProcessingScreen();

    // Send order via ESP-NOW
    ShowcaseMessage msg = createMessage(MSG_SPEND_REQUEST, g_paper4.currentUsername.c_str(),
                                        selected.cost, selected.name);

    esp_err_t result = esp_now_send(BROADCAST_MAC, (uint8_t *)&msg, sizeof(msg));

    if (result == ESP_OK) {
        Serial.printf("[OK] Order submitted: %s (-%d coins)\n", selected.name, selected.cost);
        g_paper4.balance -= selected.cost;
        g_paper4.lastOrderStatus = "✓ Order placed!";
    } else {
        Serial.println("[ERROR] Failed to send order");
        g_paper4.lastOrderStatus = "✗ Order failed!";
    }

    delay(1000);
    g_paper4.orderInProgress = false;
    g_paper4.selectedMenuIdx = -1;
    drawUI();
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
                g_paper4.currentUsername = String(msg.username);
                g_paper4.balance = 0;
                g_paper4.selectedMenuIdx = -1;
                g_paper4.lastOrderStatus = "Ready";
                Serial.printf("[OK] Identity updated: %s\n", msg.username);
                drawUI();
            }
            break;

        case MSG_EARN_COIN:
            g_paper4.balance += msg.amount;
            Serial.printf("[OK] Balance updated: %d\n", g_paper4.balance);
            drawUI();
            break;

        case MSG_SPEND_CONFIRM:
            if (msg.status == 1) {  // Success
                g_paper4.lastOrderStatus = "✓ Transaction Complete!";
                Serial.println("[OK] Transaction confirmed");
            } else {
                g_paper4.lastOrderStatus = "✗ Transaction Failed!";
                Serial.println("[WARN] Transaction failed");
            }
            drawUI();
            break;

        case MSG_RESET_ALL:
            g_paper4.currentUsername = "Guest";
            g_paper4.balance = 0;
            g_paper4.selectedMenuIdx = -1;
            g_paper4.orderInProgress = false;
            g_paper4.lastOrderStatus = "Ready";
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
    M5.begin();
    M5.Lcd.setRotation(1);
    Serial.begin(115200);
    delay(500);

    Serial.println("\n\n=== STATION 4: PAPER STARTING ===");

    // Clear screen
    M5.Lcd.fillScreen(TFT_WHITE);

    // Initialize WiFi
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);
    delay(100);

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR] ESP-NOW initialization failed");
        while (1) delay(1000);
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
    peerInfo.channel = BROADCAST_CHANNEL;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    esp_now_register_recv_cb(onDataRecv);

    Serial.println("[OK] ESP-NOW initialized");

    // Draw initial UI
    drawUI();

    Serial.println("=== STATION 4 PAPER READY ===\n");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
    M5.update();
    handleTouchInput();

    // Periodic UI refresh
    if (millis() - g_paper4.lastUpdateTime > 5000) {
        drawUI();
        g_paper4.lastUpdateTime = millis();
    }

    delay(100);
}