#include <M5Atom.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config.h"

AsyncWebServer server(80);
bool isTxInProgress = false;

void showColor(uint32_t color) {
    for (int i = 0; i < 25; i++) M5.dis.drawpix(i, color);
}

// 1. เริ่มต้น: เมื่อผู้ใช้กดปุ่ม Atom Matrix
void initiatePayment() {
    if (isTxInProgress) return;
    isTxInProgress = true;
    
    Serial.println("Tap-to-Pay: Requesting Order from Paper...");
    showColor(0x0000FF); // สีน้ำเงิน (Connecting)

    // ส่ง Request ไปถาม M5-Paper ว่าเลือกเมนูอะไรอยู่
    HTTPClient http;
    http.setTimeout(2000);
    http.begin("http://" + IP_PAPER_S4.toString() + ENDPOINT_GET_ORDER);
    
    // M5-Paper ควรตอบกลับมาที่ Endpoint /send_order ของเรา หรือ Return JSON กลับมาเลย
    // ในที่นี้เราจะให้ M5-Paper ส่งข้อมูลกลับมาที่ Endpoint ของเรา (Callback pattern)
    int code = http.POST("{}"); 
    http.end();

    if (code != 200) {
        Serial.println("Failed to reach Paper");
        showColor(0xFF0000); // แดง (Error)
        delay(1000);
        showColor(0x000000);
        isTxInProgress = false;
    }
}

// 2. รับข้อมูล Order จาก M5-Paper
void handleReceiveOrder(AsyncWebServerRequest *request) {
    if (request->hasParam("amount", true) && request->hasParam("item", true)) {
        String item = request->getParam("item", true)->value();
        int amount = request->getParam("amount", true)->value().toInt();
        
        Serial.printf("Order Received: %s (%d Coin)\n", item.c_str(), amount);
        
        // 3. ส่งคำสั่งตัดเงินไปที่ StickC
        HTTPClient http;
        String postData = "amount=" + String(amount) + "&item=" + item;
        
        http.begin("http://" + IP_STICKC.toString() + ENDPOINT_SPEND_COIN);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        int code = http.POST(postData);
        http.end();

        if (code == 200) {
            // สำเร็จ
            showColor(0x00FF00); // เขียว
            
            // สั่งเล่นเสียง Ping
            http.begin("http://" + IP_ATOM_ECHO.toString() + ENDPOINT_PLAY_TX);
            http.POST("{}");
            http.end();
        } else {
            // เงินไม่พอ หรือ Error
            showColor(0xFF0000); // แดง
        }

        request->send(200, "text/plain", "OK");
        
        delay(2000);
        showColor(0x000000);
        isTxInProgress = false;
    } else {
        request->send(400);
        isTxInProgress = false;
    }
}

void setup() {
    M5.begin(true, false, true);
    WiFi.begin(AP_SSID, AP_PASSWORD);
    
    while (WiFi.status() != WL_CONNECTED) {
        M5.dis.drawpix(0, 0xFF0000); delay(200);
        M5.dis.drawpix(0, 0x000000); delay(200);
    }
    
    WiFi.config(IP_ATOM_MATRIX_S4, IP_STATION1_AP, NETMASK, IP_STATION1_AP);
    M5.dis.drawpix(0, 0x00FF00); delay(1000); showColor(0x000000);

    // Endpoint รอรับข้อมูล Order จาก M5-Paper
    server.on(ENDPOINT_SEND_ORDER, HTTP_POST, handleReceiveOrder);
    
    server.on(ENDPOINT_HEARTBEAT, HTTP_GET, [](AsyncWebServerRequest *r){ r->send(200); });
    server.on(ENDPOINT_RESET_GLOBAL, HTTP_POST, [](AsyncWebServerRequest *r){
        showColor(0x000000);
        isTxInProgress = false;
        r->send(200);
    });

    server.begin();
}

void loop() {
    M5.update();
    if (M5.Btn.wasPressed()) {
        initiatePayment();
    }
}