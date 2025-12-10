// Station1_Core2_AP.cpp
// Hardware: M5Stack Core2
// Function: WiFi AP, DHCP, DNS, Captive Portal, User Registration
// [Updated] Added stability fixes: increased max clients, watchdog, memory monitor

#include <M5Core2.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <esp_wifi.h>
#include <esp_task_wdt.h>
#include <qrcode.h>
#include "config.h"
#include "Participant.h"

DNSServer dnsServer;
AsyncWebServer server(80);
Participant participant;

const int QR_WIDTH = 180;
const int QR_X = (320 - QR_WIDTH) / 2;

bool shouldSyncUser = false;
bool hasClientConnected = false; // [Added] Track if any client has connected
int previousClientCount = 0; // [Added] Track previous client count to detect NEW connections

unsigned long lastMemoryCheck = 0;
unsigned long lastClientCheck = 0;
const unsigned long MEMORY_CHECK_INTERVAL = 10000; // ทุก 10 วินาที
const unsigned long CLIENT_CHECK_INTERVAL = 30000;  // ทุก 30 วินาที

// Embedded HTML Code
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

// Helper: QR Code Drawing Function
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
            client.stop(); 
        } else {
            Serial.println("StickC: Connect Failed");
        }
    }

    delay(500);

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

// [Added] Helper: Print Memory Status
void printMemoryStatus() {
    Serial.printf("[MEM] Free Heap: %d bytes | Min Free: %d bytes\n", 
                  ESP.getFreeHeap(), ESP.getMinFreeHeap());
    
    // Warning if memory is low
    if (ESP.getFreeHeap() < 50000) {
        Serial.println("[WARNING] Low memory detected!");
    }
}

void printWiFiStatus() {
    wifi_sta_list_t wifi_sta_list;
    tcpip_adapter_sta_list_t adapter_sta_list;
   
    memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
    memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));
   
    esp_wifi_ap_get_sta_list(&wifi_sta_list);
    tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);
   
    Serial.printf("[WiFi] Connected Clients: %d\n", adapter_sta_list.num);
    
    for (int i = 0; i < adapter_sta_list.num; i++) {
        tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];
        Serial.printf("  Client %d - MAC: %02X:%02X:%02X:%02X:%02X:%02X | IP: " IPSTR "\n",
                     i + 1,
                     station.mac[0], station.mac[1], station.mac[2],
                     station.mac[3], station.mac[4], station.mac[5],
                     IP2STR(&station.ip));
    }
}

void drawUI() {
    M5.Lcd.fillScreen(BLACK);
    
    if (participant.Username == "") {
        M5.Lcd.setTextDatum(TC_DATUM);
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setTextFont(4);
        M5.Lcd.drawString("Station 1: Identity", 160, 10);
        
        // [Fix] Always show STEP 1 after Reset (when previousClientCount = 0)
        // Only show STEP 2 when hasClientConnected flag is explicitly set
        if (!hasClientConnected) {
            // Step 1: Show WiFi QR Code
            M5.Lcd.setTextFont(3);
            M5.Lcd.setTextColor(YELLOW, BLACK);
            M5.Lcd.drawString("STEP 1", 160, 50);
            M5.Lcd.setTextFont(2);
            M5.Lcd.drawString("Scan to Connect WiFi", 160, 40);
            
            String wifiQR = "WIFI:S:" + String(AP_SSID) + ";T:nopass;;"; 
            drawQRCode(wifiQR.c_str(), (320-145)/2, 70, 1, 5);
            
        } else {
            // Step 2: Show Portal URL QR Code
            M5.Lcd.setTextFont(3);
            M5.Lcd.setTextColor(GREEN, BLACK);
            M5.Lcd.drawString("STEP 2", 160, 50);
            M5.Lcd.setTextFont(2);
            M5.Lcd.drawString("Scan to Open Portal", 160, 40);
            
            String webQR = "http://192.168.4.1";
            drawQRCode(webQR.c_str(), (320-145)/2, 70, 1, 5);
        }
        
    } else {
        // User Created State
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setTextFont(4);
        M5.Lcd.drawString("ID Created!", 160, 100);
        
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.drawString(participant.Username, 160, 140);
        
        M5.Lcd.setTextFont(2);
        M5.Lcd.drawString("Proceed to Station 2", 160, 180);
    }
}

void setupWebHandlers() {
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("[Captive] Android /generate_204 - Returning 200 (NOT 204)");
        // Return 200 instead of 204 to trigger captive portal
        request->send(200, "text/html", "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='0;url=http://192.168.4.1/'></head><body></body></html>");
    });
    
    server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("[Captive] Android /gen_204 - Returning 200 (NOT 204)");
        request->send(200, "text/html", "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='0;url=http://192.168.4.1/'></head><body></body></html>");
    });
    
    // For devices trying to reach gstatic.com
    server.on("/mobile/status.php", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("[Captive] Android /mobile/status.php");
        request->send(200, "text/html", "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='0;url=http://192.168.4.1/'></head><body></body></html>");
    });
    
    // iOS Captive Portal Detection
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("[Captive] iOS /hotspot-detect.html");
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html);
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
    });
    
    server.on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("[Captive] iOS library test");
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html);
        request->send(response);
    });
    
    // Windows Captive Portal Detection  
    server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("[Captive] Windows /connecttest.txt");
        request->send(200, "text/plain", "Microsoft Connect Test");
    });
    
    server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("[Captive] Windows /ncsi.txt");
        request->send(200, "text/plain", "Microsoft NCSI");
    });
    
    server.on("/redirect", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("[Captive] Windows /redirect");
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html);
        request->send(response);
    });
    
    // Success endpoint
    server.on("/success.txt", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("[Captive] /success.txt check");
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html);
        request->send(response);
    });
    
    // Heartbeat for Reset-Basic Monitor
    server.on(ENDPOINT_HEARTBEAT, HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "OK");
    });
    
    // Main Portal Page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        String clientIP = request->client()->remoteIP().toString();
        Serial.printf("[Portal] Main page from: %s\n", clientIP.c_str());
        
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html);
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "0");
        request->send(response);
    });

    // Form Submit Handler
    server.on("/submit_identity", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("username", true)) {
            String inputName = request->getParam("username", true)->value();
            participant.reset();
            participant.Username = inputName;
            shouldSyncUser = true; 
            drawUI();
            
            Serial.printf("[Portal] Identity created: %s\n", inputName.c_str());
            
            AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", success_html);
            response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
            response->addHeader("Pragma", "no-cache");
            response->addHeader("Expires", "0");
            request->send(response);
        } else {
            request->send(400, "text/plain", "Missing Username");
        }
    });

    // Reset Endpoint - Optimized Version
    server.on(ENDPOINT_RESET_GLOBAL, HTTP_POST, [](AsyncWebServerRequest *request){
        Serial.println("\n>>> STATION 1 RESET TRIGGERED <<<");
        
        // [Fix] Reset participant data first - ensure Username is cleared
        participant.reset();
        participant.Username = ""; // [Added] Force clear username explicitly
        
        // [Optimized] Reset all state variables immediately
        hasClientConnected = false;
        previousClientCount = 0; // [Fix] Reset client count to prevent false detection
        
        Serial.printf("[Reset] State cleared - Username: '%s' (should be empty)\n", participant.Username.c_str());
        
        // [Optimized] Disconnect ALL clients faster - single deauth for all
        wifi_sta_list_t wifi_sta_list;
        memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
        esp_wifi_ap_get_sta_list(&wifi_sta_list);
        
        int clientCount = wifi_sta_list.num;
        if (clientCount > 0) {
            Serial.printf("[Reset] Disconnecting %d clients...\n", clientCount);
            
            // [Optimized] Single call to disconnect ALL - much faster
            esp_wifi_deauth_sta(0); // MAC = 0 means disconnect ALL clients
            delay(300); // Wait for deauth frames to be sent
            
            // [Added] Verify clients disconnected
            memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
            esp_wifi_ap_get_sta_list(&wifi_sta_list);
            Serial.printf("[Reset] Remaining clients: %d\n", wifi_sta_list.num);
        } else {
            Serial.println("[Reset] No clients to disconnect");
        }
        
        // [Fix] Force UI update - redraw MUST show STEP 1
        Serial.println("[Reset] Forcing UI redraw to STEP 1...");
        drawUI();
        
        // [Added] Verify UI state after reset
        Serial.printf("[Reset] UI redrawn - hasClientConnected: %s, Username: '%s'\n", 
                     hasClientConnected ? "true" : "false", 
                     participant.Username.c_str());
        Serial.println("[Reset] System reset complete - back to STEP 1\n");
        
        // [Improved] Better response message
        request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Station 1 reset complete\"}");
    });
    
    // Disconnect All WiFi Clients Endpoint - Optimized Version
    server.on("/disconnect_all_clients", HTTP_POST, [](AsyncWebServerRequest *request){
        Serial.println("\n>>> DISCONNECT ALL CLIENTS TRIGGERED <<<");
        
        wifi_sta_list_t wifi_sta_list;
        memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
        esp_wifi_ap_get_sta_list(&wifi_sta_list);
        
        int clientCount = wifi_sta_list.num;
        Serial.printf("[Disconnect] Found %d clients\n", clientCount);
        
        if (clientCount > 0) {
            // [Optimized] Single deauth call instead of loop
            esp_wifi_deauth_sta(0); // Disconnect ALL at once
            delay(300); // Wait for deauth frames
            
            // [Added] Verify disconnection
            memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
            esp_wifi_ap_get_sta_list(&wifi_sta_list);
            int remaining = wifi_sta_list.num;
            
            Serial.printf("[Disconnect] Disconnected %d clients, %d remaining\n", 
                         clientCount - remaining, remaining);
            
            // [Added] Force disconnect any stubborn clients
            if (remaining > 0) {
                Serial.println("[Disconnect] Force disconnecting remaining clients...");
                for (int retry = 0; retry < 3 && remaining > 0; retry++) {
                    esp_wifi_deauth_sta(0);
                    delay(200);
                    memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
                    esp_wifi_ap_get_sta_list(&wifi_sta_list);
                    remaining = wifi_sta_list.num;
                    Serial.printf("[Disconnect] Retry %d: %d clients remaining\n", retry + 1, remaining);
                }
            }
        } else {
            Serial.println("[Disconnect] No clients connected");
        }
        
        // Reset to STEP 1
        hasClientConnected = false;
        previousClientCount = 0; // [Fix] Reset counter
        drawUI();
        
        Serial.println("[Disconnect] Complete - back to STEP 1\n");
        
        request->send(200, "application/json", 
                     "{\"status\":\"success\",\"disconnected\":" + String(clientCount) + "}");
    });

    // Catch-all: แสดงหน้า Portal สำหรับทุก URL ที่ไม่รู้จัก
    server.onNotFound([](AsyncWebServerRequest *request){
        String url = request->url();
        String clientIP = request->client()->remoteIP().toString();
        Serial.printf("[Captive] Catch-all: %s from %s\n", url.c_str(), clientIP.c_str());
        
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html);
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "0");
        request->send(response);
    });
}

void setup() {
    M5.begin(true, false, true, true);
    
    Serial.begin(115200);
    Serial.println("\n\n=== Station 1 Core2 AP ===");
    Serial.println("Device is ENABLED by default");
    
    // Enable Station
    Serial.println("\n>>> ENABLING STATION 1 <<<");
    
    // [Added] Reset client tracking variables
    hasClientConnected = false;
    previousClientCount = 0;
    
    // [Modified] Disable watchdog first if exists
    esp_task_wdt_delete(NULL);
    delay(100);
    
    // Then enable new watchdog
    esp_task_wdt_init(30, true);
    esp_task_wdt_add(NULL);
    Serial.println("✓ Watchdog Timer Enabled (30s)");
    
    // [Added] Ensure clean WiFi state - disconnect any existing connections
    WiFi.mode(WIFI_OFF);
    delay(500);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IP_STATION1_AP, IP_STATION1_AP, NETMASK);
    
    // [Modified] Reduce max clients from 15 to 10 for better DHCP stability
    bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD, 1, 0, 10);
    
    if (apStarted) {
        Serial.println("✓ AP Started Successfully");
        Serial.printf("  SSID: %s\n", AP_SSID);
        Serial.printf("  IP: %s\n", WiFi.softAPIP().toString().c_str());
        Serial.printf("  Channel: 1\n");
        Serial.printf("  Max Clients: 10\n");
        
        // [Added] Set WiFi Power to Maximum for better signal strength
        esp_wifi_set_max_tx_power(84); // 84 = 21dBm (Maximum power)
        Serial.println("✓ WiFi Power set to MAXIMUM (21dBm)");
        
        esp_wifi_set_ps(WIFI_PS_NONE);
        Serial.println("✓ WiFi Power Saving Disabled");
        
        // [Added] Set WiFi bandwidth to 20MHz for better stability
        esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
        Serial.println("✓ WiFi Bandwidth set to 20MHz (stable mode)");
        
        // [Modified] Restart DHCP Server properly with proper timing
        Serial.println("  Configuring DHCP Server...");
        tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);
        delay(500); // [Increased] More time for DHCP to fully stop
        
        tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP);
        delay(500); // [Increased] More time for DHCP to fully start
        Serial.println("✓ DHCP Server Started");
        
        dnsServer.start(53, "*", IP_STATION1_AP);
        Serial.println("✓ DNS Server Started");
        
        setupWebHandlers();
        server.begin();
        Serial.println("✓ Web Server Started");
        
        printMemoryStatus();
        Serial.println("=== System Ready ===");
        Serial.println("Waiting 1 second for stabilization...\n");
        
        // [Modified] Reduce delay from 3s to 1s for faster QR code display
        delay(1000); // Just enough for DHCP to stabilize
        
        // [Added] Count initial clients (Reset-Basic, etc.)
        wifi_sta_list_t wifi_sta_list;
        tcpip_adapter_sta_list_t adapter_sta_list;
        memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
        memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));
        esp_wifi_ap_get_sta_list(&wifi_sta_list);
        tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);
        
        previousClientCount = 0;
        for (int i = 0; i < adapter_sta_list.num; i++) {
            if (adapter_sta_list.sta[i].ip.addr != 0) {
                previousClientCount++;
            }
        }
        
        Serial.printf("✓ Initial client count: %d (baseline)\n", previousClientCount);
        Serial.println("Ready for NEW clients - Captive Portal Optimized!\n");
    } else {
        Serial.println("✗ AP Failed to Start!");
        M5.Lcd.fillScreen(RED);
        M5.Lcd.setTextColor(WHITE);
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.drawString("AP START FAILED!", 160, 120);
        
        // Disable watchdog on failure
        esp_task_wdt_delete(NULL);
        return;
    }
    
    drawUI();
}

void loop() {
    M5.update();

    // [Modified] Feed watchdog
    esp_task_wdt_reset();
    
    // [Added] Periodic Memory Check
    if (millis() - lastMemoryCheck > MEMORY_CHECK_INTERVAL) {
        printMemoryStatus();
        lastMemoryCheck = millis();
    }
    
    // [Added] Periodic Client Check
    if (millis() - lastClientCheck > CLIENT_CHECK_INTERVAL) {
        printWiFiStatus();
        lastClientCheck = millis();
    }
    
    // [Modified] Check if NEW client connected (not existing clients like Reset-Basic)
    static unsigned long lastClientCountCheck = 0;
    if (millis() - lastClientCountCheck > 2000) {
        wifi_sta_list_t wifi_sta_list;
        tcpip_adapter_sta_list_t adapter_sta_list;
        
        memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
        memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));
        
        esp_wifi_ap_get_sta_list(&wifi_sta_list);
        tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);
        
        // Count clients with valid IP
        int validClientCount = 0;
        for (int i = 0; i < adapter_sta_list.num; i++) {
            if (adapter_sta_list.sta[i].ip.addr != 0) {
                validClientCount++;
            }
        }
        
        // [Fix] Only switch to STEP 2 if there's a NEW client (count increased)
        if (validClientCount > previousClientCount && !hasClientConnected) {
            hasClientConnected = true;
            Serial.printf("[UI] NEW Client connected! (%d -> %d) Switching to STEP 2 (Portal QR)\n", 
                         previousClientCount, validClientCount);
            drawUI();
        }
        
        // Update previous count
        previousClientCount = validClientCount;
        
        lastClientCountCheck = millis();
    }
    
    // Process DNS requests
    dnsServer.processNextRequest();

    if (shouldSyncUser) {
        syncUserToStations();
        shouldSyncUser = false;
    }
}
