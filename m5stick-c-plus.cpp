#include <M5Unified.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// ข้อมูล Wi-Fi ต้องตรงกับ SoftAP ของ Core2
const char *ssid_ap = "Web3Showcase_AP";
const char *password_ap = "12345678";

AsyncWebServer stickCServer(88);
String currentUsername = "Not Registered";
String authenStatus = "X";
int ccoin = 0;
String alertText = "-";

// Forward Declaration: ประกาศฟังก์ชัน updateDisplay() ไว้ที่นี่
void updateDisplay(); 

// ฟังก์ชันสำหรับวาด UI ทั้งหมดบน StickC-Plus2
void updateDisplay() {
    // 9a. แสดงข้อมูล Username และสถานะของผู้เข้าร่วม [cite: 70]
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextDatum(top_left);
    M5.Lcd.setFont(&fonts::Font2);
    
    // UI Elements [cite: 73-77]
    M5.Lcd.setCursor(5, 5);
    M5.Lcd.printf("Username: %s\n", currentUsername.c_str());
    M5.Lcd.printf("Authen Status: %s\n", authenStatus.c_str());
    M5.Lcd.printf("CCoin: %d\n", ccoin);
    
    // แสดง Alert Text
    M5.Lcd.setCursor(5, 75);
    M5.Lcd.printf("Alert Text: \n%s", alertText.c_str());
}

// Handler สำหรับรับ Username จาก Core2
void handleUsername(AsyncWebServerRequest *request) {
    if (request->hasParam("username", true)) {
        String receivedUsername = request->getParam("username", true)->value();

        // 8a. StickC-Plus2 ส่งเสียงแจ้งเตือนเมื่อได้รับข้อมูล [cite: 69, 93]
        M5.Speaker.tone(1500, 150); 
        
        // 9a. อัพเดทสถานะบนหน้าจอ [cite: 70, 93]
        currentUsername = receivedUsername;
        alertText = "Identity Created";
        authenStatus = "X"; 
        updateDisplay(); // แก้ไข: updateDisplay() ถูกเรียกใช้ได้แล้ว
        
        request->send(200, "text/plain", "Username received.");
    } else {
        request->send(400, "text/plain", "Missing Username Parameter.");
    }
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Lcd.setRotation(3); 

    // 1. เชื่อมต่อ Wi-Fi เข้ากับ SoftAP ของ Core2
    M5.Lcd.print("Connecting to Core2 AP...");
    WiFi.begin(ssid_ap, password_ap);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) { 
        delay(500);
        M5.Lcd.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        M5.Lcd.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        M5.Lcd.println("\nConnection Failed. Check Core2 AP.");
    }

    // 2. ตั้งค่า Server เพื่อรอรับ Request จาก Core2
    stickCServer.on("/set_username", HTTP_POST, handleUsername);
    stickCServer.begin();

    // แสดง UI เริ่มต้น
    updateDisplay(); 
}

void loop() {
    M5.update();
}