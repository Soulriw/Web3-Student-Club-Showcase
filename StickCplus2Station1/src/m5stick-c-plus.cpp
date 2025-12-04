#include <M5Unified.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// --- ตั้งชื่อ WiFi ที่จะปล่อย ---
const char *SSID_AP = "Web3Showcase_AP";
const char *PASSWORD_AP = NULL; // ใส่รหัสผ่านได้ถ้าต้องการ หรือ NULL เพื่อเปิดฟรี
const int LOCAL_PORT = 88; // Port 80 คือมาตรฐาน HTTP, แต่ถ้าใช้ 88 เวลาเข้าเว็บต้องพิมพ์ 192.168.4.1:88

// --- ตั้งค่า IP Address ของเครื่องนี้ ---
// ปกติ M5 จะเป็น 192.168.4.1 แต่ถ้าอยากฟิกเป็น .2 ก็ทำได้
const IPAddress local_IP(192, 168, 4, 1);     
const IPAddress gateway(192, 168, 4, 1);
const IPAddress subnet(255, 255, 255, 0);

AsyncWebServer stickCServer(LOCAL_PORT);

String currentUsername = "Not Registered";
String authenStatus = "X";
int ccoin = 0;
String alertText = "Waiting...";
String myMacAddress = "";

volatile bool needUpdate = false;

void updateDisplay() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
    
    M5.Lcd.setTextColor(CYAN);
    M5.Lcd.print("MAC: "); M5.Lcd.println(myMacAddress);
    M5.Lcd.print("IP: "); M5.Lcd.println(WiFi.softAPIP()); // โชว์ IP ให้รู้
    
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.print("User: "); M5.Lcd.println(currentUsername);
    
    M5.Lcd.setTextColor(YELLOW);
    M5.Lcd.print("Coin: "); M5.Lcd.println(ccoin);
    
    M5.Lcd.setTextColor(RED);
    M5.Lcd.println(alertText);
}

void handleUsername(AsyncWebServerRequest *request) {
    if (request->hasParam("username", true)) {
        currentUsername = request->getParam("username", true)->value();
        alertText = "ID Created!";
        needUpdate = true;
        // M5.Speaker.tone(1500, 150); 
        request->send(200, "text/plain", "OK");
    } else {
        request->send(400, "text/plain", "No User");
    }
}

void handleCoin(AsyncWebServerRequest *request) {
    if (request->hasParam("value", true)) {
        ccoin += request->getParam("value", true)->value().toInt();
        alertText = "Coin Get!";
        needUpdate = true;
        // M5.Speaker.tone(2000, 100); 
        request->send(200, "text/plain", "OK");
    } else {
        request->send(400, "text/plain", "No Value");
    }
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    
    Serial.begin(115200);
    Serial.println("--- BOOT START ---");

    M5.Lcd.setRotation(3);
    M5.Lcd.setTextSize(2); 

    // --- ส่วนที่แก้ไข: ตั้งค่าเป็น Access Point ---
    Serial.print("Setting up AP...");
    M5.Lcd.print("Creating WiFi...");
    
    WiFi.mode(WIFI_AP); // บังคับโหมด AP
    WiFi.softAPConfig(local_IP, gateway, subnet); // ตั้งค่า IP
    bool result = WiFi.softAP(SSID_AP, PASSWORD_AP); // เริ่มปล่อยสัญญาณ
    
    if(result) {
        Serial.println("Ready");
        M5.Lcd.println("Done");
    } else {
        Serial.println("Failed!");
        M5.Lcd.println("Failed");
    }

    // อ่าน MAC Address ของฝั่ง AP
    myMacAddress = WiFi.softAPmacAddress();
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("MAC: " + myMacAddress);

    // เริ่ม Server
    stickCServer.on("/set_username", HTTP_POST, handleUsername);
    stickCServer.on("/add_coin", HTTP_POST, handleCoin);
    
    // จัดการ 404 Not Found (เผื่อคนเข้าผิดหน้า)
    stickCServer.onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "text/plain", "Not found");
    });

    stickCServer.begin();
    Serial.println("Server Started");

    updateDisplay();
}

void loop() {
    M5.update();
    if (needUpdate) {
        Serial.println("Updating Display...");
        updateDisplay();
        needUpdate = false;
    }
    // ไม่ต้องมี delay ใน loop นี้เพื่อให้ AsyncWebServer ทำงานได้เต็มที่
}
