#include <M5Unified.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <qrcode.h>
#include <HTTPClient.h>
#include <DNSServer.h>

// --- CONFIGURATION ---
const char *SSID = "Web3Showcase_AP";
const char *PASSWORD = "12345678";

// IP ของ StickC (จะถูกอัปเดตอัตโนมัติเมื่อค้นเจอ)
String STICKC_IP = "192.168.4.2"; 
const int STICKC_PORT = 88;

// Network Config
IPAddress localIP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);
String localIPURL = "http://192.168.4.1";

// Server Objects
AsyncWebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;

// Application State
String penningUsername = "";
bool hasNewUserName = false;
unsigned long usernameReceivedTime = 0;
const unsigned long CONFIRMATION_DISPLAY_TIME = 3000;
bool stickCIPFound = false;

// --- HTML CONTENT ---
const char* index_html = R"raw(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Web3 Student Club Identity</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { 
            font-family: Arial, sans-serif; 
            text-align: center; 
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        .container {
            background: white;
            padding: 40px;
            border-radius: 15px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.2);
            max-width: 400px;
            width: 90%;
        }
        h1 { 
            color: #333; 
            margin-bottom: 10px;
            font-size: 24px;
        }
        .subtitle {
            color: #666;
            margin-bottom: 30px;
            font-size: 14px;
        }
        label {
            display: block;
            color: #333;
            margin-bottom: 10px;
            font-weight: bold;
        }
        input[type=text] { 
            padding: 12px; 
            margin-bottom: 20px; 
            border-radius: 5px; 
            border: 2px solid #ddd;
            width: 100%;
            font-size: 16px;
            transition: border-color 0.3s;
        }
        input[type=text]:focus {
            outline: none;
            border-color: #667eea;
        }
        button { 
            background-color: #667eea;
            color: white; 
            cursor: pointer;
            padding: 12px;
            margin: 5px 0;
            border-radius: 5px;
            border: none;
            width: 100%;
            font-size: 16px;
            font-weight: bold;
            transition: background-color 0.3s;
        }
        button:hover {
            background-color: #764ba2;
        }
        button:active {
            transform: scale(0.98);
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🎯 STATION 1</h1>
        <p class="subtitle">Identity Creation Portal</p>
        <form action="/submit" method="POST">
            <label for="username">Enter Your Username:</label>
            <input type="text" id="username" name="username" placeholder="e.g. Satoshi_N" required autofocus>
            <button type="submit">✓ Submit & Connect</button>
        </form>
    </div>
</body>
</html>
)raw";

// --- CORE FUNCTIONS ---

// 1. [UI] แสดง QR Code
void displayQRCode(const char* data) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextDatum(top_center);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setFont(&fonts::Font4); 
    M5.Lcd.drawString("1. Scan to Connect", M5.Lcd.width() / 2, 10); 

    int size = 180;
    // จัดกึ่งกลาง QR Code
    M5.Lcd.qrcode(data, (M5.Lcd.width() - size) / 2, 50, size);
    
    M5.Lcd.setTextDatum(top_left);
    M5.Lcd.setCursor(5, 240);
    M5.Lcd.setFont(&fonts::Font2);
    M5.Lcd.setTextColor(CYAN);
    M5.Lcd.print("Then enter your username");
}

// 2. [UI] แสดงหน้า Confirmation
void displayConfirmation(String username) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextDatum(middle_center);
    M5.Lcd.setFont(&fonts::Font4);
    M5.Lcd.setTextColor(GREEN);
    
    M5.Lcd.drawString("SUCCESS!", M5.Lcd.width() / 2, 80);
    
    M5.Lcd.setFont(&fonts::Font2);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.drawString("Username Registered:", M5.Lcd.width() / 2, 140);
    
    M5.Lcd.setFont(&fonts::Font4);
    M5.Lcd.setTextColor(CYAN);
    M5.Lcd.drawString(username.c_str(), M5.Lcd.width() / 2, 180);
    
    M5.Lcd.setFont(&fonts::Font2);
    M5.Lcd.setTextColor(YELLOW);
    M5.Lcd.drawString("Check your M5StickC-Plus2", M5.Lcd.width() / 2, 240);
}

// 3. [SYSTEM] ค้นหา StickC IP (Non-blocking DNS Fix)
void discoverStickCIP() {
    Serial.println("[Discovery] Attempting to find StickC-Plus2...");
    
    for (int i = 2; i <= 20; i++) { 
        // 🚨 CRITICAL: ประมวลผล DNS ระหว่างรอ เพื่อไม่ให้ Android Timeout
        dnsServer.processNextRequest(); 
        
        String testIP = "192.168.4." + String(i);
        String url = "http://" + testIP + ":" + String(STICKC_PORT) + "/get_info";
        
        HTTPClient http;
        http.begin(url);
        // Timeout สั้นๆ เพื่อให้ Loop เร็วขึ้น
        http.setConnectTimeout(200); 
        
        int httpResponseCode = http.GET();
        if (httpResponseCode == 200) {
            STICKC_IP = testIP;
            stickCIPFound = true;
            Serial.printf("[Discovery] ✓ Found StickC-Plus2 at: %s\n", STICKC_IP.c_str());
            
            // UI Feedback
            M5.Lcd.fillScreen(BLACK);
            M5.Lcd.setTextDatum(middle_center);
            M5.Lcd.setFont(&fonts::Font2);
            M5.Lcd.setTextColor(GREEN);
            M5.Lcd.drawString("StickC Found!", M5.Lcd.width() / 2, M5.Lcd.height() / 2 - 20);
            M5.Lcd.setTextColor(CYAN);
            M5.Lcd.drawString(STICKC_IP.c_str(), M5.Lcd.width() / 2, M5.Lcd.height() / 2 + 20);
            
            delay(1500);

            // กลับไปแสดง QR Code
            String qrData = "WIFI:S:" + String(SSID) + ";T:WPA;P:" + String(PASSWORD) + ";;";
            displayQRCode(qrData.c_str());
            
            http.end();
            return; // เจอแล้ว ออกจากฟังก์ชันทันที
        }
        http.end();
        
        // ประมวลผล DNS อีกครั้งหลังจบ Request
        dnsServer.processNextRequest();
    }
    
    Serial.println("[Discovery] ✗ StickC-Plus2 not found (Scanning...)");
    stickCIPFound = false;
}

// 4. [SYSTEM] ส่ง Username ไป StickC
void sendUsernameToStickC(String username) {
    if (!stickCIPFound) {
        Serial.println("[HTTP] StickC IP unknown. Cannot send.");
        return; 
    }
    
    HTTPClient http;
    String url = "http://" + STICKC_IP + ":" + String(STICKC_PORT) + "/set_username"; 

    http.begin(url);
    http.setConnectTimeout(2000); 
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String postData = "username=" + username;

    int httpResponseCode = http.POST(postData);

    M5.Lcd.setTextDatum(top_left);
    M5.Lcd.setCursor(5, 100);
    M5.Lcd.setFont(&fonts::Font2);
    
    if (httpResponseCode > 0) {
        M5.Lcd.setTextColor(GREEN);
        M5.Lcd.printf("✓ Sent to StickC (Code: %d)", httpResponseCode);
        Serial.printf("[HTTP] Username sent successfully (Code: %d)\n", httpResponseCode);
    } else {
        M5.Lcd.setTextColor(RED);
        M5.Lcd.printf("✗ Failed to send");
        Serial.printf("[HTTP] Failed to send username (Error: %s)\n", http.errorToString(httpResponseCode).c_str());
    }
    http.end();
}

// 5. [SETUP] Config Web Server & Captive Portal
void setUpWebserver() {
    // A. Captive Portal Redirects (ทำให้ Android/iOS เด้งหน้า Login)
    server.on("/generate_204", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });
    server.on("/gen_204", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });
    server.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });
    server.on("/wpad.dat", [](AsyncWebServerRequest *request) { request->send(404); }); 
    server.on("/redirect", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });
    server.on("/hotspot-detect.html", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });
    server.on("/canonical.html", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });
    server.on("/success.txt", [](AsyncWebServerRequest *request) { request->send(200, "text/plain", "success"); });
    server.on("/ncsi.txt", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });
    server.on("/favicon.ico", [](AsyncWebServerRequest *request) { request->send(404); });

    // B. Serve Root HTML
    server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html);
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "0");
        request->send(response);
        Serial.println("Served Portal Page");
    });

    // C. Handle Form Submission
    server.on("/submit", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("username", true)) {
            String username = request->getParam("username", true)->value();
            
            // Sanitize input
            for (int i = 0; i < username.length(); i++) {
                char c = username[i];
                if (!isalnum(c) && c != '_' && c != '-') username[i] = '_';
            }
            
            penningUsername = username;
            hasNewUserName = true;
            usernameReceivedTime = millis();
            Serial.printf("[Form] Username: %s\n", username.c_str());
            
            // ส่งหน้า Success กลับไปให้มือถือ
            String successHtml = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'></head><body style='text-align:center;margin-top:50px;font-family:Arial;'><h1>✓ Success!</h1><p>Username registered.</p></body></html>";
            request->send(200, "text/html", successHtml);
        } else {
            request->send(400, "text/html", "Error: Missing Username");
        }
    });

    // D. Catch All - Redirect ทุกอย่างที่ไม่รู้จักไปหน้าแรก
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->redirect(localIPURL); 
    });

    server.begin();
    Serial.println("\nHTTP Server Started.");
}

// --- MAIN SETUP ---
void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Lcd.setRotation(1); 
    Serial.begin(115200); 

    // 1. Setup WiFi AP
    const int MAX_CONNECTIONS = 10;
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(localIP, gateway, subnet);
    // เปิด SoftAP พร้อมกำหนด Max Connections
    WiFi.softAP(SSID, PASSWORD, 1, false, MAX_CONNECTIONS);
    
    // Display AP Info
    M5.Lcd.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    M5.Lcd.printf("AP MAC: %s\n", WiFi.softAPmacAddress().c_str());

    // 2. Setup DNS & WebServer
    dnsServer.start(DNS_PORT, "*", localIP);
    setUpWebserver();

    // 3. Show Initial UI
    String qrData = "WIFI:S:" + String(SSID) + ";T:WPA;P:" + String(PASSWORD) + ";;";
    displayQRCode(qrData.c_str());
    
    Serial.println("[Setup] Core2 ready!");
}

// --- MAIN LOOP ---
void loop() {
    // 1. จัดการ DNS Request ตลอดเวลา (สำคัญมากสำหรับ Captive Portal)
    dnsServer.processNextRequest();

    // 2. ถ้ายังหา StickC ไม่เจอ ให้ค้นหา (แบบ Non-blocking DNS)
    if (!stickCIPFound) {
        discoverStickCIP();
        // ไม่ต้อง return; เพื่อให้ loop ทำงานต่อได้ (แต่ discoverStickCIP จะกินเวลาหน่อย)
    }

    // 3. ถ้ามี Username ใหม่เข้ามา ให้ส่งไป StickC
    if(hasNewUserName){
        sendUsernameToStickC(penningUsername);
        hasNewUserName = false;
        usernameReceivedTime = millis();
    }
    
    // 4. จัดการหน้าจอ Confirmation
    if (usernameReceivedTime > 0 && millis() - usernameReceivedTime < CONFIRMATION_DISPLAY_TIME) {
        displayConfirmation(penningUsername);
    } else if (usernameReceivedTime > 0) {
        // หมดเวลาโชว์ Confirmation -> กลับมาหน้า QR Code
        usernameReceivedTime = 0;
        String qrData = "WIFI:S:" + String(SSID) + ";T:WPA;P:" + String(PASSWORD) + ";;";
        displayQRCode(qrData.c_str());
    }
    
    M5.update();
}