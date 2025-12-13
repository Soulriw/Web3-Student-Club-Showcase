// Station1_Core2_AP.cpp
// Hardware: M5Stack Core2
// Function: WiFi AP, DHCP, DNS, Captive Portal, User Registration
// [Optimized for iOS & Android compatibility]

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

bool shouldSyncUser = false;
bool hasClientConnected = false;
int previousClientCount = 0;

unsigned long lastMemoryCheck = 0;
const unsigned long MEMORY_CHECK_INTERVAL = 30000; // Check every 30s

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

void drawQRCode(const char* text, int x_start, int y_start, int size, int scale) {
    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(4)];
    qrcode_initText(&qrcode, qrcodeData, 4, ECC_MEDIUM, text);

    int qr_size = qrcode.size;
    int border = 10;
    M5.Lcd.fillRect(x_start - border, y_start - border, 
                    qr_size * scale + (border * 2), qr_size * scale + (border * 2), WHITE);

    for (uint8_t y = 0; y < qr_size; y++) {
        for (uint8_t x = 0; x < qr_size; x++) {
            if (qrcode_getModule(&qrcode, x, y)) {
                M5.Lcd.fillRect(x_start + (x * scale), y_start + (y * scale), scale, scale, BLACK);
            }
        }
    }
}

void syncUserToStations() {
    String postData = "username=" + participant.Username;
    Serial.printf("[Sync] Sending username: '%s'\n", participant.Username.c_str());

    // Sync to StickC
    {
        WiFiClient client;
        HTTPClient http;
        http.setReuse(false);
        http.setTimeout(3000);
        
        if (http.begin(client, "http://" + IP_STICKC.toString() + ENDPOINT_SET_USER)) {
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            int code = http.POST(postData);
            Serial.printf("[Sync] StickC: %d\n", code > 0 ? code : -1);
            http.end();
            client.stop();
        }
    }

    delay(300);

    // Sync to Station 2
    {
        WiFiClient client;
        HTTPClient http;
        http.setReuse(false);
        http.setTimeout(3000);
        
        if (http.begin(client, "http://" + IP_STATION2_MON.toString() + ENDPOINT_SET_USER)) {
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            int code = http.POST(postData);
            Serial.printf("[Sync] Station2: %d\n", code > 0 ? code : -1);
            http.end();
            client.stop();
        }
    }
}

void drawUI() {
    M5.Lcd.fillScreen(BLACK);
    
    if (participant.Username == "") {
        M5.Lcd.setTextDatum(TC_DATUM);
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setTextFont(4);
        M5.Lcd.drawString("Station 1: Identity", 160, 10);
        
        if (!hasClientConnected) {
            M5.Lcd.setTextFont(2);
            M5.Lcd.setTextColor(YELLOW, BLACK);
            M5.Lcd.drawString("STEP 1: Connect WiFi", 160, 45);
            
            String wifiQR = "WIFI:S:" + String(AP_SSID) + ";T:WPA;P:" + String(AP_PASSWORD) + ";;";
            drawQRCode(wifiQR.c_str(), (320-165)/2, 70, 1, 5);
        } else {
            M5.Lcd.setTextFont(2);
            M5.Lcd.setTextColor(GREEN, BLACK);
            M5.Lcd.drawString("STEP 2: Open Portal", 160, 45);
            
            drawQRCode("http://192.168.4.1", (320-165)/2, 70, 1, 5);
        }
    } else {
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setTextFont(4);
        M5.Lcd.drawString("Identity Created!", 160, 100);
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.drawString(participant.Username, 160, 140);
        M5.Lcd.setTextFont(2);
        M5.Lcd.drawString("Go to Station 2", 160, 180);
    }
}

void setupWebHandlers() {
    // iOS captive portal detection
    server.on("/hotspot-detect.html", HTTP_ANY, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", index_html);
    });

    // Main portal
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", index_html);
    });

    // Form submission
    server.on("/submit_identity", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("username", true)) {
            participant.Username = request->getParam("username", true)->value();
            shouldSyncUser = true;
            drawUI();
            request->send(200, "text/html", success_html);
        } else {
            request->send(400, "text/plain", "Missing username");
        }
    });

    // Heartbeat
    server.on(ENDPOINT_HEARTBEAT, HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "OK");
    });

    // Global reset
    server.on(ENDPOINT_RESET_GLOBAL, HTTP_POST, [](AsyncWebServerRequest *request){
        Serial.println("[Reset] Triggered");
        participant.reset();
        participant.Username = "";
        hasClientConnected = false;
        previousClientCount = 0;
        
        // Disconnect all clients by restarting AP
        String ssid = String(AP_SSID);
        String pass = String(AP_PASSWORD);
        WiFi.softAPdisconnect(true);
        delay(100);
        WiFi.softAP(ssid.c_str(), pass.c_str(), 6, 0, 4);
        delay(200);
        
        drawUI();
        request->send(200, "application/json", "{\"status\":\"success\"}");
    });

    // Catch-all
    server.onNotFound([](AsyncWebServerRequest *request){
        request->send(200, "text/html", index_html);
    });
}

void setup() {
    M5.begin(true, false, true, true);
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n=== Station 1: Identity Creation ===");
    
    // Clean WiFi state
    WiFi.mode(WIFI_OFF);
    delay(500);
    WiFi.mode(WIFI_AP);
    delay(100);
    
    // Configure IP
    if (!WiFi.softAPConfig(IP_STATION1_AP, IP_STATION1_AP, NETMASK)) {
        Serial.println("[ERROR] Failed to configure AP IP");
        M5.Lcd.fillScreen(RED);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.drawString("IP Config Failed", 160, 120);
        while(1) delay(1000);
    }
    
    // Start AP with minimal config for maximum compatibility
    Serial.printf("[AP] Starting: %s\n", AP_SSID);
    bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD, 6, 0, 10);
    
    if (!apStarted) {
        Serial.println("[ERROR] AP failed to start");
        M5.Lcd.fillScreen(RED);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.drawString("AP Start Failed", 160, 120);
        while(1) delay(1000);
    }
    
    Serial.printf("[AP] SSID: %s\n", AP_SSID);
    Serial.printf("[AP] Pass: %s\n", AP_PASSWORD);
    Serial.printf("[AP] IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("[AP] Channel: 6\n");
    Serial.printf("[AP] Max Clients: 10\n");
    
    // Start DNS (captive portal)
    dnsServer.setTTL(0);
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    if (!dnsServer.start(53, "*", IP_STATION1_AP)) {
        Serial.println("[WARN] DNS start failed");
    } else {
        Serial.println("[DNS] Started");
    }
    
    // Start web server
    setupWebHandlers();
    server.begin();
    Serial.println("[HTTP] Started");
    
    Serial.printf("[MEM] Free: %d bytes\n", ESP.getFreeHeap());
    Serial.println("=== Ready ===\n");
    
    delay(1000);
    drawUI();
}

void loop() {
    M5.update();
    
    // Process DNS frequently for captive portal
    dnsServer.processNextRequest();
    
    // Check for new clients
    static unsigned long lastClientCheck = 0;
    if (millis() - lastClientCheck > 2000) {
        wifi_sta_list_t sta_list;
        memset(&sta_list, 0, sizeof(sta_list));
        esp_wifi_ap_get_sta_list(&sta_list);
        
        if (sta_list.num > previousClientCount && !hasClientConnected) {
            hasClientConnected = true;
            Serial.printf("[Client] New connection detected (%d total)\n", sta_list.num);
            drawUI();
        }
        previousClientCount = sta_list.num;
        lastClientCheck = millis();
    }
    
    // Memory check
    if (millis() - lastMemoryCheck > MEMORY_CHECK_INTERVAL) {
        Serial.printf("[MEM] Free: %d bytes\n", ESP.getFreeHeap());
        lastMemoryCheck = millis();
    }
    
    // Sync user if needed
    if (shouldSyncUser) {
        syncUserToStations();
        shouldSyncUser = false;
    }
    
    dnsServer.processNextRequest();
    delay(10);
}
