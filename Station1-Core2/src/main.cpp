// Station1_Core2_AP.cpp
// Hardware: M5Stack Core2
// Function: WiFi AP, DHCP, DNS, Captive Portal, User Registration
// [Updated] Added Enable/Disable mode to prevent dual AP collision

#include <M5Core2.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <esp_wifi.h>
#include <qrcode.h>
#include "config.h"
#include "Participant.h"

DNSServer dnsServer;
AsyncWebServer server(80);
Participant participant;

const int QR_WIDTH = 180;
const int QR_X = (320 - QR_WIDTH) / 2;

// [Added] Device Enable/Disable State
bool isStationActive = false;  // เริ่มต้นปิด ต้องกดเปิดก่อนใช้งาน

// Flag เพื่อบอก loop() ว่าต้องส่งข้อมูล
bool shouldSyncUser = false;

// --- Embedded HTML Code ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Web3 Student Club</title>
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; text-align: center; background-color: #f4f4f9; padding-top: 50px; margin: 0; }
        .container { background: white; max-width: 350px; margin: 0 auto; padding: 30px; border-radius: 15px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
        h1 { color: #333; margin-bottom: 20px; font-size: 24px; }
        p { color: #666; margin-bottom: 30px; }
        input[type="text"] { width: 100%; padding: 12px; margin-bottom: 20px; border: 1px solid #ddd; border-radius: 8px; box-sizing: border-box; font-size: 16px; }
        button { background-color: #007bff; color: white; padding: 14px 20px; border: none; border-radius: 8px; cursor: pointer; width: 100%; font-size: 18px; font-weight: bold; transition: background 0.3s; }
        button:hover { background-color: #0056b3; }
        .footer { margin-top: 20px; font-size: 12px; color: #999; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Web3 Student Club</h1>
        <p>Enter your unique username to generate your event identity.</p>
        
        <form action="/submit_identity" method="POST">
            <input type="text" name="username" placeholder="Username (e.g. Alice)" required minlength="3" autofocus>
            <button type="submit">Create Identity</button>
        </form>
        
        <div class="footer">Station 1: Identity Creation</div>
    </div>
</body>
</html>
)rawliteral";

const char success_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: sans-serif; text-align: center; padding-top: 50px; background-color: #e8f5e9; }
        .card { background: white; padding: 30px; border-radius: 15px; display: inline-block; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
        h1 { color: #2e7d32; }
        p { font-size: 18px; color: #333; }
    </style>
</head>
<body>
    <div class="card">
        <h1>Identity Created!</h1>
        <p>Please check your Wearable.</p>
        <p>Proceed to Station 2.</p>
    </div>
</body>
</html>
)rawliteral";

// --- Helper: QR Code Drawing Function ---
void drawQRCode(const char* text, int x_start, int y_start, int size, int scale) {
    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(3)];
    qrcode_initText(&qrcode, qrcodeData, 3, 0, text);

    int qr_size = qrcode.size;
    int scaled_size = qr_size * scale;
    
    M5.Lcd.fillRect(x_start - 5, y_start - 5, scaled_size + 10, scaled_size + 10, WHITE);

    for (uint8_t y = 0; y < qr_size; y++) {
        for (uint8_t x = 0; x < qr_size; x++) {
            if (qrcode_getModule(&qrcode, x, y)) {
                M5.Lcd.fillRect(x_start + (x * scale), y_start + (y * scale), scale, scale, BLACK);
            }
        }
    }
}

// --- Helper: Send Data to other Stations ---
void syncUserToStations() {
    String postData = "username=" + participant.Username;
    
    Serial.printf("Sending Username: '%s'\n", participant.Username.c_str());

    // [Fix] แยก Scope เพื่อบังคับให้สร้าง Connection ใหม่ทุกครั้ง ไม่ Reuse
    // --- 1. Sync to StickC ---
    {
        WiFiClient client;
        HTTPClient http;
        http.setReuse(false); // [Added] ป้องกันการใช้ Socket เก่าซ้ำ
        http.setTimeout(3000); // [Increased] เพิ่มเวลา Timeout
        
        Serial.println("Syncing user to StickC...");
        if (http.begin(client, "http://" + IP_STICKC.toString() + ENDPOINT_SET_USER)) {
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            int code = http.POST(postData);
            
            if (code > 0) {
                Serial.printf("StickC Response: %d (Success)\n", code);
            } else {
                Serial.printf("StickC Failed: %s\n", http.errorToString(code).c_str());
            }
            http.end(); 
            client.stop(); // [Added] บังคับปิด Socket ทันที
        } else {
            Serial.println("StickC: Connect Failed");
        }
    }

    delay(500); // [Increased] พักนานขึ้นเพื่อให้ Network Stack คืนทรัพยากร (สำคัญ!)

    // --- 2. Sync to Station 2 ---
    {
        WiFiClient client;
        HTTPClient http;
        http.setReuse(false); // [Added]
        http.setTimeout(3000); // [Increased]
        
        Serial.println("Syncing user to Station 2...");
        if (http.begin(client, "http://" + IP_STATION2_MON.toString() + ENDPOINT_SET_USER)) {
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            int code = http.POST(postData);
            
            if (code > 0) {
                Serial.printf("Station2 Response: %d (Success)\n", code);
            } else {
                Serial.printf("Station2 Failed: %s\n", http.errorToString(code).c_str());
            }
            http.end();
            client.stop(); // [Added]
        } else {
             Serial.println("Station2: Connect Failed");
        }
    }
}

void drawUI() {
    M5.Lcd.fillScreen(BLACK);
    
    // [Added] Show Disabled State
    if (!isStationActive) {
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.setTextColor(RED, BLACK);
        M5.Lcd.setTextFont(4);
        M5.Lcd.drawString("Station 1: DISABLED", 160, 80);
        
        M5.Lcd.setTextColor(YELLOW, BLACK);
        M5.Lcd.setTextFont(2);
        M5.Lcd.drawString("Press BUTTON A to ENABLE", 160, 120);
        M5.Lcd.drawString("(Only enable ONE Station1)", 160, 140);
        
        // Bottom Button Labels
        M5.Lcd.fillRect(0, 200, 320, 40, DARKGREY);
        M5.Lcd.setTextColor(WHITE, DARKGREY);
        M5.Lcd.drawString("ENABLE", 53, 220);
        
        return;
    }
    
    // [Existing] Normal Operation UI
    if (participant.Username == "") {
        M5.Lcd.setTextDatum(TC_DATUM);
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setTextFont(4);
        M5.Lcd.drawString("Station 1: Identity", 160, 10);
        
        String wifiQR = "WIFI:S:" + String(AP_SSID) + ";T:nopass;;"; 
        drawQRCode(wifiQR.c_str(), (320-145)/2, 50, 1, 5); 
        
        M5.Lcd.setTextFont(2);
        M5.Lcd.drawString("Connect WiFi & Create ID", 160, 210);
        
        // Bottom Button
        M5.Lcd.fillRect(0, 200, 107, 40, DARKGREY);
        M5.Lcd.setTextColor(RED, DARKGREY);
        M5.Lcd.drawString("DISABLE", 53, 220);
        
    } else {
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setTextFont(4);
        M5.Lcd.drawString("ID Created!", 160, 100);
        
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.drawString(participant.Username, 160, 140);
        
        M5.Lcd.setTextFont(2);
        M5.Lcd.drawString("Proceed to Station 2", 160, 180);
        
        // Bottom Button
        M5.Lcd.fillRect(0, 200, 107, 40, DARKGREY);
        M5.Lcd.setTextColor(RED, DARKGREY);
        M5.Lcd.drawString("DISABLE", 53, 220);
    }
}

void setupWebHandlers() {
    auto redirect = [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    };
    
    server.on("/generate_204", redirect);
    server.on("/redirect", redirect);
    server.on("/hotspot-detect.html", redirect);
    server.on("/canonical.html", redirect);
    server.on("/ncsi.txt", redirect);
    
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });

    server.on("/submit_identity", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("username", true)) {
            // [Fix] รับค่ามาก่อน
            String inputName = request->getParam("username", true)->value();
            
            // [Fix] Reset ข้อมูลเก่าทิ้ง
            participant.reset();
            
            // [Fix] แล้วค่อยใส่ค่าใหม่ลงไป (ไม่งั้น reset จะล้างทิ้งหมด)
            participant.Username = inputName;
            
            shouldSyncUser = true; 
            drawUI();
            request->send_P(200, "text/html", success_html);
        } else {
            request->send(400, "text/plain", "Missing Username");
        }
    });

    server.on(ENDPOINT_RESET_GLOBAL, HTTP_POST, [](AsyncWebServerRequest *request){
        participant.reset();
        drawUI();
        request->send(200, "text/plain", "S1 Reset");
    });

    server.onNotFound(redirect);
}

void setup() {
    M5.begin(true, false, true, true);
    
    Serial.begin(115200);
    Serial.println("\n\n=== Station 1 Core2 AP ===");
    Serial.println("Device is DISABLED by default");
    Serial.println("Press Button A to ENABLE this Station");
    
    drawUI();
}

void loop() {
    M5.update();
    
    // [Added] Button A: Enable/Disable Station
    if (M5.BtnA.wasPressed()) {
        if (!isStationActive) {
            // Enable Station
            Serial.println("\n>>> ENABLING STATION 1 <<<");
            
            WiFi.mode(WIFI_AP);
            WiFi.softAPConfig(IP_STATION1_AP, IP_STATION1_AP, NETMASK);
            
            bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD, 1, 0, 4);
            
            if (apStarted) {
                Serial.println("✓ AP Started Successfully");
                Serial.printf("  SSID: %s\n", AP_SSID);
                Serial.printf("  IP: %s\n", WiFi.softAPIP().toString().c_str());
                Serial.printf("  Channel: 1\n");
                
                dnsServer.start(53, "*", IP_STATION1_AP);
                Serial.println("✓ DNS Server Started");
                
                setupWebHandlers();
                server.begin();
                Serial.println("✓ Web Server Started");
                Serial.println("=== System Ready ===\n");
                
                isStationActive = true;
            } else {
                Serial.println("✗ AP Failed to Start!");
                M5.Lcd.fillScreen(RED);
                M5.Lcd.setTextColor(WHITE);
                M5.Lcd.setTextDatum(MC_DATUM);
                M5.Lcd.drawString("AP START FAILED!", 160, 120);
                return;
            }
            
        } else {
            // Disable Station
            Serial.println("\n>>> DISABLING STATION 1 <<<");
            
            server.end();
            dnsServer.stop();
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_OFF);
            
            participant.reset();
            isStationActive = false;
            
            Serial.println("✓ Station Disabled\n");
        }
        
        drawUI();
    }
    
    // [Modified] Only process if station is active
    if (isStationActive) {
        dnsServer.processNextRequest();

        if (shouldSyncUser) {
            syncUserToStations();
            shouldSyncUser = false;
        }
    }
}
