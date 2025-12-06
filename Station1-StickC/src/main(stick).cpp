#include <M5Unified.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <ProjectConfig.h>

AsyncWebServer server(80);

// Global State
String currentUsername = "Guest";
String authStatus = "X";
int ccoin = 0;

void updateScreen(String alert = "") {
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(5, 10);
    M5.Display.setTextSize(1.5); // StickC จอเล็ก
    M5.Display.printf("User: %s\n", currentUsername.c_str());
    M5.Display.printf("Auth: %s\n", authStatus.c_str());
    M5.Display.printf("Coin: %d\n", ccoin);
    
    if(alert != "") {
        M5.Display.setTextColor(YELLOW);
        M5.Display.setCursor(5, 80);
        M5.Display.print(alert);
        M5.Display.setTextColor(WHITE);
    }
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(3);
    M5.Display.setTextSize(2);
    M5.Display.print("Connecting...");

    // Fixed IP for StickC using ProjectConfig.h IPAddress constants
    IPAddress subnet(255, 255, 255, 0);
    WiFi.config(IP_STICKC, IP_CORE2, subnet);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    while (WiFi.status() != WL_CONNECTED) delay(500);
    updateScreen("Ready");

    // 1. รับ Identity จาก Station 1
    server.on("/update_identity", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, 
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        StaticJsonDocument<200> doc; deserializeJson(doc, data);
        currentUsername = doc["username"].as<String>();
        authStatus = "X"; ccoin = 0;
        M5.Speaker.tone(1000, 200);
        updateScreen("Registered!");
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // 2. Auth Process จาก Station 2
    server.on("/auth_start", HTTP_POST, [](AsyncWebServerRequest *request){
        updateScreen("AUTH...\nIn Progress");
        request->send(200);
    });
    server.on("/auth_success", HTTP_POST, [](AsyncWebServerRequest *request){
        authStatus = "/";
        M5.Speaker.tone(2000, 200);
        updateScreen("Auth Success!");
        request->send(200);
    });

    // 3. Add Coin จาก Station 3
    server.on("/add_coin", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, 
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        StaticJsonDocument<200> doc; deserializeJson(doc, data);
        int amount = doc["amount"];
        ccoin += amount;
        M5.Speaker.tone(3000, 100); M5.Speaker.tone(4000, 100);
        updateScreen("Got " + String(amount) + " Coins");
        request->send(200);
    });

    // 4. Spend Coin จาก Station 4
    server.on("/spend_coin", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, 
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        StaticJsonDocument<200> doc; deserializeJson(doc, data);
        int cost = doc["cost"];
        String item = doc["item"].as<String>();

        if(ccoin >= cost) {
            ccoin -= cost;
            updateScreen("Paid for\n" + item);
            request->send(200); // OK
        } else {
            M5.Speaker.tone(200, 500);
            updateScreen("Not Enough\nCoin!");
            request->send(400); // Fail
        }
    });

    // 5. Reset
    server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request){
        currentUsername = "Guest"; authStatus = "X"; ccoin = 0;
        updateScreen("Reset Done");
        request->send(200);
    });

    server.begin();
}

void loop() { M5.update(); }