// Audio_AtomEcho.cpp: โค้ดสำหรับ M5-Atom Echo (Audio Beacons/Client)
// รับ Request เพื่อเล่นเสียงแจ้งเตือนการยืนยันตัวตนและธุรกรรมสำเร็จ

#include <M5Atom.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "config.h"

AsyncWebServer server(80);

// --- ฟังก์ชันการเล่นเสียง ---

void playAuthSound() {
    Serial.println("Playing Authentication Sound...");
    // 14. เสียงแจ้งเตือนการยืนยันตัวตน: (Beep สั้น 2 ครั้ง เว้น 1 วิ, Beep ยาว 1 วินาที)
    
    // Beep สั้น 1
    M5.Speaker.tone(1000, 100); 
    delay(100);
    M5.Speaker.end();
    
    delay(1000); // เว้น 1 วิ
    
    // Beep สั้น 2
    M5.Speaker.tone(1000, 100);
    delay(100);
    M5.Speaker.end();
    
    delay(1000); // เว้น 1 วิ
    
    // Beep ยาว 1
    M5.Speaker.tone(1500, 1000); // 1 วินาที
    delay(1000);
    M5.Speaker.end();
    
    Serial.println("Auth sound finished. Sending success signal.");
    
    // 15. ส่ง Request แจ้งสถานะสำเร็จไปยัง Core2, StickC-Plus2
    // ใช้ Atom Echo เป็นตัวส่งสัญญาณความสำเร็จของกระบวนการ
    
    // Note: Atom Echo does not have an easy way to send HTTP requests with M5Atom library,
    // For simplicity and stability, we assume it can send the post request. 
    // In a real environment, this logic would live on the Atom Matrix which has more resources.
    // However, following the flow: Atom Echo -> Core2, StickC-Plus2
    
    HTTPClient http;
    // Notify Core2 (Monitor)
    http.begin(String("http://") + IP_CORE2.toString() + ENDPOINT_SET_AUTH);
    http.POST("{}");
    http.end();
    
    // Notify StickC-Plus2 (Wearable)
    http.begin(String("http://") + IP_STICKC.toString() + ENDPOINT_SET_AUTH);
    http.POST("{}");
    http.end();
}

void playTxSuccessSound() {
    Serial.println("Playing Transaction Success Sound...");
    // 30. เล่นเสียงแจ้งเตือนธุรกรรมสำเร็จ (ใช้ Beep สั้น 3 ครั้ง)
    M5.Speaker.tone(1200, 50); delay(50); M5.Speaker.end();
    delay(50);
    M5.Speaker.tone(1200, 50); delay(50); M5.Speaker.end();
    delay(50);
    M5.Speaker.tone(1500, 100); delay(100); M5.Speaker.end();
}

// --- ฟังก์ชัน HTTP Server Handlers ---

void handlePlayAuth(AsyncWebServerRequest *request) {
    playAuthSound();
    request->send(200, "text/plain", "Auth sound playing.");
}

void handlePlayTx(AsyncWebServerRequest *request) {
    playTxSuccessSound();
    request->send(200, "text/plain", "Tx sound playing.");
}

void handleSystemReset(AsyncWebServerRequest *request) {
    M5.Speaker.end(); // หยุดเสียงที่กำลังเล่น
    request->send(200, "text/plain", "Echo reset complete.");
}

// --- Setup Function ---
void setup() {
    M5.begin(true, false, true); // Atom Echo: Init, Power=false, Serial=true
    Serial.begin(115200);

    // กำหนด Static IP
    WiFi.config(IP_ATOM_ECHO, IP_CORE2, IPAddress(255, 255, 255, 0));
    WiFi.begin(AP_SSID, AP_PASSWORD);

    Serial.println("Connecting to AP...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.print("\nConnected to AP. IP: ");
    Serial.println(WiFi.localIP());

    // ตั้งค่า Server Endpoints
    server.on(ENDPOINT_PLAY_AUTH, HTTP_POST, handlePlayAuth);
    server.on(ENDPOINT_PLAY_TX_SUCCESS, HTTP_POST, handlePlayTx);
    server.on(ENDPOINT_RESET_USER, HTTP_POST, handleSystemReset);

    server.begin();
    Serial.println("HTTP server started on Atom Echo.");

    M5.dis.setLed(0, 0x00FF00); // Green light on successful connection
}

void loop() {
    M5.update();
    // Speaker is handled in the callback functions
    delay(100);
}