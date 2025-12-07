// Station4_Paper.cpp: โค้ดสำหรับ M5-Paper (Menu Display/Spend Tokens)
// จัดการ E-Ink Display, Touch Input, และส่ง Order Info กลับไปยัง Atom Matrix

#include <M5EPD.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include "config.h"

AsyncWebServer server(80);

// ข้อมูลเมนู
struct Menu {
    String name;
    int coin;
};

Menu menuList[] = {
    {"Coffee", 2},
    {"Croissant", 3},
    {"Lunch Set", 5},
    {"Tea", 2}
};
const int NUM_MENU = 4;

int selectedMenuIndex = -1;

// ตำแหน่งของปุ่มและข้อความ (ค่าประมาณ)
const int START_Y_MENU = 100;
const int ROW_HEIGHT_MENU = 50;

// --- ฟังก์ชันช่วยเหลือด้าน UI (E-Ink) ---
void drawMenuDashboard(m5epd_update_mode_t mode = UPDATE_MODE_GC16) {
    M5.EPD.WriteFullWindow(M5EPD_RECT_WINDOW_W, M5EPD_RECT_WINDOW_H, mode); // clear display

    M5.EPD.SetFont(&AsciiFont8);
    M5.EPD.SetTextColor(0x0000, 0xFFFF); // Black text on White background

    // Title
    M5.EPD.DrawString("4. Spend Tokens", 50, 20);
    M5.EPD.DrawString("CAMT WEB3 CAFE", 50, 60);

    // Menu List
    for (int i = 0; i < NUM_MENU; i++) {
        String text = String(i + 1) + ". " + menuList[i].name + " .... " + String(menuList[i].coin) + " CCoin";
        
        // Highlight selection
        if (i == selectedMenuIndex) {
            M5.EPD.FillRect(50, START_Y_MENU + i * ROW_HEIGHT_MENU, 540, ROW_HEIGHT_MENU, 0x0000); // Black background
            M5.EPD.SetTextColor(0xFFFF, 0x0000); // White text
        } else {
            M5.EPD.FillRect(50, START_Y_MENU + i * ROW_HEIGHT_MENU, 540, ROW_HEIGHT_MENU, 0xFFFF); // White background
            M5.EPD.SetTextColor(0x0000, 0xFFFF); // Black text
        }
        
        M5.EPD.DrawString(text.c_str(), 60, START_Y_MENU + i * ROW_HEIGHT_MENU + (ROW_HEIGHT_MENU / 2) - 10);
    }
    
    M5.EPD.UpdateFull(mode);
}

// --- ฟังก์ชัน Touch Handler ---
void handleTouch(int x, int y) {
    // Check Menu List Area
    for (int i = 0; i < NUM_MENU; i++) {
        if (x > 50 && x < 590 && y > START_Y_MENU + i * ROW_HEIGHT_MENU && y < START_Y_MENU + (i + 1) * ROW_HEIGHT_MENU) {
            selectedMenuIndex = i;
            Serial.printf("Menu selected: %s\n", menuList[i].name.c_str());
            // 24. อนุญาตให้เลือกเมนูผ่าน Touch Screen
            drawMenuDashboard(UPDATE_MODE_DU4);
            return;
        }
    }
}

// --- ฟังก์ชัน HTTP Server Handlers ---
// Endpoint สำหรับรับ Request Order จาก Atom Matrix (Tap-to-Pay)
void handleGetOrder(AsyncWebServerRequest *request) {
    if (selectedMenuIndex != -1) {
        // 25. ส่งข้อมูล Order ไปยัง Atom Matrix
        Menu selected = menuList[selectedMenuIndex];
        String body = "{\"item\": \"" + selected.name + "\", \"amount\": " + String(selected.coin) + "}";
        
        // ส่ง Order Info กลับไปยัง Atom Matrix (Endpoint '/order_info' on Atom Matrix)
        HTTPClient http;
        String url = String("http://") + IP_ATOM_MATRIX.toString() + "/order_info";
        http.begin(url);
        http.addHeader("Content-Type", "application/json");
        http.POST(body);
        http.end();
        
        // Clear selection to prevent double payment
        selectedMenuIndex = -1;
        drawMenuDashboard(UPDATE_MODE_DU4);

        request->send(200, "text/plain", "Order information sent to Atom Matrix.");
    } else {
        request->send(400, "text/plain", "No menu selected.");
    }
}

void handleSystemReset(AsyncWebServerRequest *request) {
    selectedMenuIndex = -1;
    // 52. รีเซ็ตการเลือกเมนู
    drawMenuDashboard();
    request->send(200, "text/plain", "M5-Paper S4 reset complete.");
}

// --- Setup Function ---
void setup() {
    M5.begin();
    M5.EPD.SetRotation(90); // Landscape
    Serial.begin(115200);

    // กำหนด Static IP
    WiFi.config(IP_PAPER_S4, IP_CORE2, IPAddress(255, 255, 255, 0));
    WiFi.begin(AP_SSID, AP_PASSWORD);

    Serial.println("Connecting to AP...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.print("\nConnected to AP. IP: ");
    Serial.println(WiFi.localIP());

    // ตั้งค่า Server Endpoints
    server.on(ENDPOINT_GET_ORDER, HTTP_POST, handleGetOrder);
    server.on(ENDPOINT_RESET_MENU, HTTP_POST, handleSystemReset);

    server.begin();

    // 23. แสดง Menu List
    drawMenuDashboard();
}

void loop() {
    M5.update();
    
    // Handle Touch Input
    if (M5.TP.avaliable()) {
        M5.TP.Get>>tpData;
        if (tpData.touches != 0) {
            handleTouch(tpData.points[0].x, tpData.points[0].y);
        }
        tpData.touches = 0;
    }
    
    delay(50);
}