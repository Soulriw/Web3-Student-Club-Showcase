#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <esp_gap_ble_api.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "config.h"
#include "Participant.h"

AsyncWebServer server(80);
Participant participant;
String alertMsg = "Ready";

// [DEBUG] ตัวแปร Flag เพื่อบอกให้ Loop หลักวาดหน้าจอ
volatile bool needUpdateUI = false;

// --- Data Structure (ต้องเหมือนกับฝั่งส่ง) ---
typedef struct struct_message {
    char type[10];      // เช่น "TRIGGER", "AUTH", "USER"
    char username[50];  
    int status;
} struct_message;

struct_message incomingData;

// --- [BLE] ตัวแปร Global ---
BLEServer *pServer = NULL;
BLEAdvertising *pAdvertising = NULL;
bool shouldRestartBLE = false;
String newBLEName = "";

// ------------------------------------------------------
// 📥 ESP-NOW Callback (Logic ใหม่ + Signature ที่ถูกต้อง)
// ------------------------------------------------------
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingDataPtr, int len) {
    if (len == sizeof(struct_message)) {
        memcpy(&incomingData, incomingDataPtr, sizeof(struct_message));
        
        Serial.printf("[ESP-NOW] Recv Type: %s | User: %s\n", incomingData.type, incomingData.username);

        // --- Case 1: ยืนยันตัวตนสำเร็จ (จาก Atom Echo) ---
        if (strcmp(incomingData.type, "AUTH") == 0) {
            participant.isAuthenticated = true;
            alertMsg = "Auth by Echo"; // หรือ "Auth Success" ตามชอบ
            
            M5.Speaker.tone(4000, 200); 
            needUpdateUI = true; 
            Serial.println(">> AUTH SUCCESS VIA ESP-NOW <<");
        }
        // --- Case 2: ลงทะเบียนชื่อผู้ใช้ (จาก Core2) ---
        else if (strcmp(incomingData.type, "USER") == 0) {
            // 1. บันทึกชื่อ
            participant.reset();
            participant.Username = String(incomingData.username);
            
            // 2. อัปเดตข้อความ
            alertMsg = "ID Received";
            
            // 3. สั่งเปลี่ยนชื่อ BLE
            newBLEName = participant.Username;
            shouldRestartBLE = true;

            // 4. แจ้งเตือน
            M5.Speaker.tone(2000, 200);
            needUpdateUI = true;
            Serial.printf(">> USER REGISTERED: %s <<\n", participant.Username.c_str());
        }
    }
}

// ------------------------------------------------------
// BLE Functions
// ------------------------------------------------------
void startBLE(String name) {
    if (pAdvertising == NULL) {
        BLEDevice::init(name.c_str());
        pServer = BLEDevice::createServer();
        pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->setScanResponse(true);
        pAdvertising->setMinPreferred(0x06); 
        pAdvertising->setMinPreferred(0x12);
    } else {
        pAdvertising->stop();
    }

    esp_ble_gap_set_device_name(name.c_str());

    BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
    oAdvertisementData.setFlags(0x06);
    oAdvertisementData.setCompleteServices(BLEUUID("1234")); 
    pAdvertising->setAdvertisementData(oAdvertisementData);

    BLEAdvertisementData oScanResponseData = BLEAdvertisementData();
    oScanResponseData.setName(name.c_str());
    pAdvertising->setScanResponseData(oScanResponseData);

    pAdvertising->start();
    Serial.printf("[BLE] Name Updated to: %s\n", name.c_str());
}

// ------------------------------------------------------
// 🎨 UI Function (แบบเดิม: พื้นหลังดำ)
// ------------------------------------------------------
void drawUI() {
    M5.Lcd.fillScreen(BLACK); // กลับมาใช้สีดำ
    
    // Battery Status
    int bat = M5.Power.getBatteryLevel();
    M5.Lcd.setTextSize(1);
    M5.Lcd.setFont(&fonts::Font2);
    
    M5.Lcd.setCursor(180, 5);
    if (bat > 20) M5.Lcd.setTextColor(GREEN, BLACK);
    else M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.printf("%d%%", bat);

    // Info: Username
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setFont(&fonts::Font4); 
    
    M5.Lcd.setCursor(5, 30);
    // ถ้ายังไม่มีชื่อ ให้ขึ้น -
    String displayName = (participant.Username.length() > 0) ? participant.Username : "-";
    M5.Lcd.printf("U: %s", displayName.c_str());
    
    // Info: Auth Status
    M5.Lcd.setCursor(5, 60);
    String st = participant.isAuthenticated ? "YES" : "NO";
    if (participant.isAuthenticated) M5.Lcd.setTextColor(GREEN, BLACK);
    else M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.printf("Auth: %s", st.c_str());

    // Info: Coin
    M5.Lcd.setCursor(5, 90);
    M5.Lcd.setTextColor(YELLOW, BLACK);
    M5.Lcd.printf("Coin: %d", participant.CCoin_Balance);
    
    // Info: Alert Message
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(5, 115);
    M5.Lcd.setFont(&fonts::Font2);
    M5.Lcd.printf("%s", alertMsg.c_str());
}

// ------------------------------------------------------
// SETUP
// ------------------------------------------------------
void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Lcd.setRotation(1); // แนวนอนแบบเดิม
    
    Serial.begin(115200);
    Serial.println("\n\n--- StickC System Booting ---");

    // UI เริ่มต้น (Loading)
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setFont(&fonts::Font2);
    M5.Lcd.setCursor(10, 50);
    M5.Lcd.print("Connecting WiFi...");
    
    // WiFi Setup
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(AP_SSID, AP_PASSWORD);
    
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
        delay(500);
        retry++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.config(IP_STICKC, IP_STATION1_AP, NETMASK, IP_STATION1_AP);
        Serial.println("\nWiFi Connected!");
        Serial.print("IP: "); Serial.println(WiFi.localIP());
        Serial.print("Channel: "); Serial.println(WiFi.channel());
    } else {
        Serial.println("\n[WIFI] Connect Failed! (Offline Mode)");
    }

    // ESP-NOW Init
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Init Failed");
        M5.Lcd.println("ESP-NOW Fail");
    } else {
        esp_now_register_recv_cb(OnDataRecv);
        Serial.println("[ESP-NOW] Ready");
    }

    // --- Web Server Endpoints ---
    
    // 1. Endpoint สำหรับ Matrix
    server.on("/auth_start", HTTP_GET, [](AsyncWebServerRequest *r){
        alertMsg = "AUTH... In Progress"; 
        needUpdateUI = true;
        r->send(200, "text/plain", "ACK");
    });

    // 2. Endpoint ตั้งค่า User ผ่าน HTTP (Backup)
    server.on(ENDPOINT_SET_USER, HTTP_POST, [](AsyncWebServerRequest *r){
        if (r->hasParam("username", true)) {
            String val = r->getParam("username", true)->value();
            participant.reset();
            participant.Username = val;
            alertMsg = "ID Received";
            
            newBLEName = val;
            shouldRestartBLE = true; 
            needUpdateUI = true;
            r->send(200);
        } else r->send(400);
    });

    // Endpoint Reset
    server.on(ENDPOINT_RESET_GLOBAL, HTTP_POST, [](AsyncWebServerRequest *r){
        participant.reset();
        alertMsg = "Ready";
        newBLEName = "GUEST";
        shouldRestartBLE = true;
        needUpdateUI = true;
        r->send(200);
    });

    server.begin();
    startBLE("GUEST");
    drawUI(); 
}

// ------------------------------------------------------
// LOOP
// ------------------------------------------------------
void loop() {
    M5.update();
    
    if (needUpdateUI) {
        drawUI();
        needUpdateUI = false; 
    }

    if (shouldRestartBLE) {
        startBLE(newBLEName);
        shouldRestartBLE = false; 
    }

    // Refresh แบตเตอรี่ทุก 5 วิ
    static long lastBat = 0;
    if (millis() - lastBat > 5000) {
        // อัปเดตเฉพาะแบตเตอรี่ หรือเรียก drawUI() ก็ได้ถ้าไม่ซีเรียสเรื่องกระพริบ
        // เพื่อความชัวร์ เรียก drawUI ไปเลยก็ได้ครับสำหรับ StickC
        // drawUI(); 
        lastBat = millis();
    }
}