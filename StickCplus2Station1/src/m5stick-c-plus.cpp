#include <M5Unified.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <BLEDevice.h> 
#include <BLEUtils.h>
#include <BLEServer.h>

// --- CENTRALIZED CONFIGURATION ---
const char *SSID_AP = "Web3Showcase_AP";
const char *PASSWORD_AP = "12345678";
const int LOCAL_PORT = 88; 

// --- GLOBALS ---
AsyncWebServer stickCServer(LOCAL_PORT);
String currentUsername = "Not Registered";
String authenStatus = "X"; 
int ccoin = 0;
String alertText = "Waiting for Identity...";
String myMacAddress = "";

// 🚩 Flags สำหรับส่งงานไปทำใน loop() (แก้ WDT Reset)
bool flagNewUsername = false; 
bool flagAuthStart = false;
bool flagAuthComplete = false;
bool flagSetCoins = false;

// ตัวแปรชั่วคราวรับค่าจาก Handler
String tempUsername = "";
String tempSenderMac = "";
int tempCoins = 0;

// ตัวแปร BLE
BLEServer *pServer = NULL;
BLEAdvertising *pAdvertising = NULL;

// --- FUNCTIONS ---

void startBLE(String name) {
    if (pServer != NULL) {
        BLEDevice::deinit(true);
    }
    BLEDevice::init(name.c_str()); 
    pServer = BLEDevice::createServer();
    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID("1234"); 
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.printf("[BLE] Advertising Started as: %s\n", name.c_str());
}

void updateDisplay() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextDatum(top_left);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextColor(WHITE);
    
    M5.Display.setCursor(5, 5); M5.Display.print("MAC: "); M5.Display.println(myMacAddress);
    
    M5.Display.setCursor(5, 25); M5.Display.print("User: ");
    M5.Display.setTextColor(currentUsername == "Not Registered" ? ORANGE : GREEN);
    M5.Display.println(currentUsername);
    
    M5.Display.setCursor(5, 50); M5.Display.setTextColor(WHITE); M5.Display.print("Status: ");
    M5.Display.setTextColor(authenStatus == "✓" ? GREEN : RED);
    M5.Display.println(authenStatus);
    
    M5.Display.setCursor(5, 75); M5.Display.setTextColor(YELLOW); M5.Display.printf("CCoin: %d\n", ccoin);
    
    M5.Display.setCursor(5, 100); M5.Display.setTextColor(MAGENTA); M5.Display.printf("MSG: %s", alertText.c_str());
}

void handleGetInfo(AsyncWebServerRequest *request) {
    String response = "{\"status\":\"StickC-Plus2 Ready\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
    request->send(200, "application/json", response);
}

// **[HANDLER]** รับ Username (ทำงานเร็วที่สุดเท่าที่จะทำได้)
void handleUsername(AsyncWebServerRequest *request) {
    if (request->hasParam("username", true)) {
        // 1. รับค่าและเก็บใส่ตัวแปรชั่วคราว
        tempUsername = request->getParam("username", true)->value();
        
        // 2. ยกธงบอก loop() ว่ามีงานเข้า
        flagNewUsername = true; 
        
        // 3. ตอบกลับทันที (ไม่ทำ Display/Sound ตรงนี้)
        request->send(200, "text/plain", "OK");
        Serial.println("[HTTP] Username received -> Flag Set");
    } else {
        request->send(400, "text/plain", "Fail");
    }
}

// **[HANDLER]** Auth Start
void handleAuthStart(AsyncWebServerRequest *request) {
    tempSenderMac = request->hasParam("sender_mac") ? request->getParam("sender_mac")->value() : "Unknown";
    flagAuthStart = true; // ยกธง
    request->send(200, "text/plain", "Auth Started");
}

// **[HANDLER]** Auth Complete
void handleAuthComplete(AsyncWebServerRequest *request) {
    tempSenderMac = request->hasParam("sender_mac") ? request->getParam("sender_mac")->value() : "Unknown";
    flagAuthComplete = true; // ยกธง
    request->send(200, "text/plain", "Auth Completed");
}

// **[HANDLER]** Set Coins
void handleSetCoins(AsyncWebServerRequest *request) {
    if (request->hasParam("coins", true)) {
        tempCoins = request->getParam("coins", true)->value().toInt();
        flagSetCoins = true; // ยกธง
        request->send(200, "text/plain", "OK");
    } else {
        request->send(400, "text/plain", "Fail");
    }
}

// --- SETUP ---
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(3); 
    
    M5.Display.setBrightness(128);
    M5.Display.wakeup(); // ใช้ wakeup แทน power(true) เพื่อความชัวร์

    // WiFi
    M5.Display.fillScreen(BLACK);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextColor(CYAN);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString("Connecting WiFi...", M5.Display.width()/2, M5.Display.height()/2);
    
    WiFi.mode(WIFI_STA); 
    WiFi.begin(SSID_AP, PASSWORD_AP);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) { 
        delay(500);
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        myMacAddress = WiFi.macAddress();
    }

    // Server
    stickCServer.on("/get_info", HTTP_GET, handleGetInfo);
    stickCServer.on("/set_username", HTTP_POST, handleUsername); 
    stickCServer.on("/set_coins", HTTP_POST, handleSetCoins); 
    stickCServer.on("/auth_start", HTTP_GET, handleAuthStart);   
    stickCServer.on("/auth_complete", HTTP_GET, handleAuthComplete); 
    stickCServer.begin();
    
    startBLE("GUEST-PLAYER");
    updateDisplay();
}

// --- LOOP ---
// งานหนักทั้งหมดทำที่นี่ (ปลอดภัยจาก WDT)
void loop() {
    M5.update();
    
    // 1. Process New Username
    if (flagNewUsername) {
        flagNewUsername = false; // เอาธงลง
        
        currentUsername = tempUsername;
        Serial.printf("[LOOP] Processing Username: %s\n", currentUsername.c_str());
        
        M5.Speaker.tone(1500, 150); 
        delay(200); // delay ใน loop ปลอดภัยกว่าใน handler
        M5.Speaker.tone(2000, 300);
        
        alertText = "Identity Confirmed!";
        authenStatus = "X";
        updateDisplay(); 
        
        // BLE Init (งานหนักที่สุด)
        startBLE(currentUsername); 
        Serial.println("[LOOP] BLE Updated");
    }

    // 2. Process Auth Start
    if (flagAuthStart) {
        flagAuthStart = false;
        M5.Speaker.tone(800, 100);
        alertText = "AUTH... (via " + tempSenderMac + ")";
        authenStatus = "?";
        updateDisplay();
    }

    // 3. Process Auth Complete
    if (flagAuthComplete) {
        flagAuthComplete = false;
        M5.Speaker.tone(1500, 200);
        authenStatus = "✓";
        alertText = "Authentication SUCCESS!";
        updateDisplay();
    }

    // 4. Process Set Coins
    if (flagSetCoins) {
        flagSetCoins = false;
        ccoin = tempCoins;
        alertText = "CCoin updated!";
        updateDisplay();
    }
    
    delay(10); // พักเล็กน้อยให้ CPU เย็นลง
}