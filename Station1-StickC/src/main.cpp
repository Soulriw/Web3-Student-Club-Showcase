#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "config.h"
#include "Participant.h"

AsyncWebServer server(80);
Participant participant;
String alertMsg = "Ready";

void drawUI() {
    M5.Lcd.fillScreen(BLACK);
    
    // Battery Status
    int bat = M5.Power.getBatteryLevel();
    M5.Lcd.setTextSize(1);
    
    // แก้ไข: ใช้ Font มาตรฐานแทน Font ภาษาไทยที่หาไม่เจอ
    M5.Lcd.setFont(&fonts::Font2); // Font ขนาดกลาง อ่านง่าย
    
    M5.Lcd.setCursor(180, 5);
    if (bat > 20) M5.Lcd.setTextColor(GREEN, BLACK);
    else M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.printf("%d%%", bat);

    // Info
    M5.Lcd.setTextColor(WHITE, BLACK);
    // M5.Lcd.setFont(&fonts::lgfxThai_24); // <-- บรรทัดที่มีปัญหา (คอมเมนต์ออก)
    M5.Lcd.setFont(&fonts::Font4); // ใช้อันนี้แทน (ตัวใหญ่ หนา ชัดเจน)
    
    M5.Lcd.setCursor(5, 30);
    M5.Lcd.printf("U: %s", participant.Username.c_str());
    
    M5.Lcd.setCursor(5, 60);
    String st = participant.isAuthenticated ? "OK" : "NO";
    if (participant.isAuthenticated) M5.Lcd.setTextColor(GREEN, BLACK);
    else M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.printf("Auth: %s", st.c_str());

    M5.Lcd.setCursor(5, 90);
    M5.Lcd.setTextColor(YELLOW, BLACK);
    M5.Lcd.printf("Coin: %d", participant.CCoin_Balance);
    
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(5, 115);
    M5.Lcd.setFont(&fonts::Font2); // กลับมาใช้ตัวเล็กสำหรับข้อความยาวๆ
    M5.Lcd.printf("%s", alertMsg.c_str());
}

void setup() {
    M5.begin();
    M5.Lcd.setRotation(1);
    
    // เชื่อมต่อ WiFi
    M5.Lcd.print("Connecting...");
    WiFi.begin(AP_SSID, AP_PASSWORD);
    
    // รอจนกว่าจะเชื่อมต่อได้
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
        delay(500);
        M5.Lcd.print(".");
        retry++;
    }
    
    // ตั้งค่า IP
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.config(IP_STICKC, IP_STATION1_AP, NETMASK, IP_STATION1_AP);
    }

    // Endpoints
    server.on(ENDPOINT_HEARTBEAT, HTTP_GET, [](AsyncWebServerRequest *r){ r->send(200); });
    
    server.on(ENDPOINT_SET_USER, HTTP_POST, [](AsyncWebServerRequest *r){
        if (r->hasParam("username", true)) {
            participant.Username = r->getParam("username", true)->value();
            participant.reset();
            alertMsg = "ID Received";
            M5.Speaker.tone(2000, 100);
            drawUI();
            r->send(200);
        } else r->send(400);
    });

    // Handle Auth
    server.on(ENDPOINT_SET_AUTH, HTTP_POST, [](AsyncWebServerRequest *r){
        participant.isAuthenticated = true;
        alertMsg = "Auth Success";
        M5.Speaker.tone(1000, 100); // Beep
        drawUI();
        r->send(200);
    });

    // Handle Earn
    server.on(ENDPOINT_EARN_COIN, HTTP_POST, [](AsyncWebServerRequest *r){
        if (r->hasParam("amount", true)) {
            int amt = r->getParam("amount", true)->value().toInt();
            participant.CCoin_Balance += amt;
            alertMsg = "Earned " + String(amt);
            M5.Speaker.tone(3000, 100);
            drawUI();
            r->send(200);
        } else r->send(400);
    });

    // Handle Spend
    server.on(ENDPOINT_SPEND_COIN, HTTP_POST, [](AsyncWebServerRequest *r){
        if (r->hasParam("amount", true)) {
            int amt = r->getParam("amount", true)->value().toInt();
            if (participant.CCoin_Balance >= amt) {
                participant.CCoin_Balance -= amt;
                alertMsg = "Spent " + String(amt);
                M5.Speaker.tone(4000, 100);
                drawUI();
                r->send(200);
            } else {
                alertMsg = "No Coin!";
                M5.Speaker.tone(500, 500); // Error sound
                drawUI();
                r->send(402, "text/plain", "Insufficient funds");
            }
        } else r->send(400);
    });
    
    server.on(ENDPOINT_RESET_GLOBAL, HTTP_POST, [](AsyncWebServerRequest *r){
        participant.reset();
        alertMsg = "Reset";
        drawUI();
        r->send(200);
    });

    server.begin();
    drawUI();
}

void loop() {
    M5.update();
    static long lastBat = 0;
    if (millis() - lastBat > 5000) {
        drawUI(); // Refresh battery
        lastBat = millis();
    }
}