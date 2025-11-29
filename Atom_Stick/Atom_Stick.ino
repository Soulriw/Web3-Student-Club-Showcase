#include <M5Unified.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// --- CONFIGURATION ---
const char *SSID_AP = "Web3Showcase_AP";
const char *PASSWORD_AP = "12345678";
const int LOCAL_PORT = 88; 

// --- FIX IP SETTINGS (จำเป็นต้องมี เพื่อให้ Core2 ส่งถูกที่) ---
IPAddress localIP(192, 168, 4, 2);   
IPAddress gateway(192, 168, 4, 1);   
IPAddress subnet(255, 255, 255, 0); 

// --- GLOBALS ---
AsyncWebServer stickCServer(LOCAL_PORT);
String currentUsername = "Not Registered";
String authenStatus = "X";
int ccoin = 0;
String alertText = "Waiting for Identity...";
String myMacAddress = "";

// ตัวแปรสำหรับ BLE
BLEServer *pServer = NULL;
BLEAdvertising *pAdvertising = NULL;

// --- FUNCTIONS ---

// 1. ฟังก์ชันเริ่มปล่อยสัญญาณ BLE (เปลี่ยนชื่อตาม User + ใส่รหัสลับ 1234)
void startBLE(String name) {
    // ถ้าเคยเปิด BLE ไว้อยู่ ให้ปิดก่อนเพื่อรีเซ็ตชื่อ
    if (pServer != NULL) {
        BLEDevice::deinit(true);
        delay(100);
    }

    // เริ่ม BLE ด้วยชื่อใหม่
    BLEDevice::init(name.c_str()); 
    pServer = BLEDevice::createServer();
    pAdvertising = BLEDevice::getAdvertising();
    
    // ใส่ Service UUID "1234" (เพื่อให้ Matrix หาเจอ ไม่ว่าชื่อจะเป็นอะไรก็ตาม)
    pAdvertising->addServiceUUID("1234"); 
    
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    
    Serial.printf("[BLE] Advertising Started as: %s\n", name.c_str());
}

// 2. ฟังก์ชันอัปเดตหน้าจอ
void updateDisplay() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextDatum(top_left);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextColor(WHITE);
    
    M5.Display.setCursor(5, 5);
    M5.Display.printf("IP: %s\n", WiFi.localIP().toString().c_str()); // โชว์ IP
    M5.Display.setCursor(5, 25);
    M5.Display.printf("User: %s\n", currentUsername.c_str());
    M5.Display.printf("Status: %s\n", authenStatus.c_str());
    M5.Display.printf("CCoin: %d\n", ccoin);
    
    // เปลี่ยนสีข้อความแจ้งเตือนตามสถานะ
    if (authenStatus == "/") M5.Display.setTextColor(GREEN);
    else M5.Display.setTextColor(ORANGE);
    
    M5.Display.setCursor(5, 95);
    M5.Display.printf("MSG: %s", alertText.c_str());
}

// 3. Handler รับ Username จาก Core2
void handleUsername(AsyncWebServerRequest *request) {
    // รองรับทั้ง GET และ POST
    String receivedUsername = "";
    if (request->hasParam("username", true)) {
        receivedUsername = request->getParam("username", true)->value();
    } else if (request->hasParam("username", false)) {
        receivedUsername = request->getParam("username", false)->value();
    }

    if (receivedUsername != "") {
        // แจ้งเตือนเสียง
        M5.Speaker.tone(1500, 150); 
        delay(100);
        M5.Speaker.tone(2000, 300);

        // Debug Log
        Serial.printf("[HTTP] Received Username: %s\n", receivedUsername.c_str());

        // อัปเดตข้อมูล
        currentUsername = receivedUsername;
        alertText = "Identity Confirmed!";
        authenStatus = "/"; 
        
        // ⚠️ สำคัญ: เปลี่ยนชื่อ Bluetooth เป็นชื่อผู้ใช้ทันที
        startBLE(receivedUsername);
        
        updateDisplay(); 
        
        // ตอบกลับ Core2 ว่าได้รับแล้ว (200 OK)
        request->send(200, "text/plain", "OK");
    } else {
        request->send(400, "text/plain", "Fail: Missing Username");
    }
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(3); 
    
    Serial.begin(115200);
    // --- Phase 1: Connecting WiFi ---
    M5.Display.fillScreen(BLACK);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextColor(WHITE);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString("Connecting WiFi...", M5.Display.width()/2, M5.Display.height()/2 - 10);
    
    // ⚠️ Config IP ก่อน connect เสมอ (สำคัญมาก)
    WiFi.config(localIP, gateway, subnet);
    WiFi.begin(SSID_AP, PASSWORD_AP);
    
    // Debug Log
    Serial.println("[WiFi] Connecting...");
    
    // Loop รอการเชื่อมต่อ พร้อมแสดงจุด . . .
    M5.Display.setTextDatum(top_left);
    M5.Display.setCursor(10, M5.Display.height()/2 + 10);
    while (WiFi.status() != WL_CONNECTED) { 
        delay(500);
        M5.Display.print("."); 
        Serial.print(".");
    }
    Serial.println("\n[WiFi] Connected!");
    
    // --- Phase 2: System Ready ---
    myMacAddress = WiFi.macAddress();
    
    // เริ่ม Server
    stickCServer.on("/set_username", HTTP_POST, handleUsername);
    stickCServer.on("/set_username", HTTP_GET, handleUsername); 
    stickCServer.begin();

    // เริ่ม BLE ครั้งแรก (ชื่อ Default)
    startBLE("GUEST-PLAYER");

    // แสดงหน้าจอหลัก
    updateDisplay();
}

void loop() {
    M5.update();
}