#include <M5Atom.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include "config.h"

AsyncWebServer server(80);
bool isAuthInProgress = false;

// ฟังก์ชันเปลี่ยนสีไฟ (5x5 Matrix)
void showColor(uint32_t color) {
    for (int i = 0; i < 25; i++) M5.dis.drawpix(i, color);
}

// ฟังก์ชันเริ่มกระบวนการ Auth
void triggerAuth() {
    if (isAuthInProgress) return;
    isAuthInProgress = true;
    
    Serial.println("Triggering Auth...");
    showColor(0x0000FF); // สีน้ำเงิน (Processing)

    HTTPClient http;
    http.setTimeout(2000);

    // 1. สั่ง Atom Echo เล่นเสียง
    http.begin("http://" + IP_ATOM_ECHO.toString() + ENDPOINT_PLAY_AUTH);
    http.POST("{}");
    http.end();

    // 2. สั่ง StickC ให้เปลี่ยนสถานะ
    http.begin("http://" + IP_STICKC.toString() + ENDPOINT_SET_AUTH);
    http.POST("{}");
    http.end();

    // 3. สั่ง Core2 Monitor ให้โชว์ Success
    http.begin("http://" + IP_STATION2_MON.toString() + ENDPOINT_SET_AUTH);
    http.POST("{}");
    http.end();

    delay(1000);
    showColor(0x00FF00); // สีเขียว (Success)
    delay(2000);
    showColor(0x000000); // ปิดไฟ
    isAuthInProgress = false;
}

void setup() {
    M5.begin(true, false, true); // Init Atom (LED=Enable, Serial=Enable)
    WiFi.begin(AP_SSID, AP_PASSWORD);
    
    // รอเชื่อมต่อ WiFi (ไฟสีแดงกะพริบ)
    while (WiFi.status() != WL_CONNECTED) {
        M5.dis.drawpix(0, 0xFF0000); delay(200);
        M5.dis.drawpix(0, 0x000000); delay(200);
    }
    // เชื่อมต่อแล้ว (ไฟสีเขียวแวบหนึ่ง)
    WiFi.config(IP_ATOM_MATRIX_S2, IP_STATION1_AP, NETMASK, IP_STATION1_AP);
    M5.dis.drawpix(0, 0x00FF00); delay(1000); showColor(0x000000);

    server.on(ENDPOINT_HEARTBEAT, HTTP_GET, [](AsyncWebServerRequest *r){ r->send(200); });
    server.on(ENDPOINT_RESET_GLOBAL, HTTP_POST, [](AsyncWebServerRequest *r){
        showColor(0x000000); // Reset = ปิดไฟ
        isAuthInProgress = false;
        r->send(200);
    });
    server.begin();
}

void loop() {
    M5.update();
    
    // 1. Manual Trigger: กดปุ่มหน้าจอเพื่อ Auth (เผื่อ RSSI ไม่แม่น)
    if (M5.Btn.wasPressed()) {
        triggerAuth();
    }

    // 2. RSSI Trigger: ตรวจจับสัญญาณ StickC
    static unsigned long lastScan = 0;
    if (millis() - lastScan > 3000 && !isAuthInProgress) { // Scan ทุก 3 วิ
        int n = WiFi.scanNetworks();
        for (int i = 0; i < n; ++i) {
            // เช็คว่าเจอ Mac Address หรือ SSID ของ StickC หรือไม่ 
            // (ใน SoftAP Mode Client จะไม่ปล่อย SSID, ดังนั้นใช้ปุ่มกดจะเสถียรกว่ามากสำหรับงาน Showcase)
        }
        lastScan = millis();
    }
}