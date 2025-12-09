// Reset_CoreBasic.cpp
// Hardware: M5Stack Core Basic
// Function: System Monitor (Ping all devices) & Global Reset Button
// [Updated] Added Serial debug, WiFi stability fixes, and improved HTTP handling

#include <M5Stack.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "config.h"

struct Device {
    String name;
    IPAddress ip;
    bool online;
    int lastResponseTime;
};

Device devices[] = {
    {"S1: AP/Identity", IP_STATION1_AP, false, 0},
    {"S2: Monitor", IP_STATION2_MON, false, 0},
    {"Wearable", IP_STICKC, false, 0},
    {"Atom Sensor", IP_ATOM_MATRIX, false, 0},
    {"Atom Echo", IP_ATOM_ECHO, false, 0},
    {"Paper Earn", IP_PAPER_S3, false, 0},
    {"Paper Spend", IP_PAPER_S4, false, 0}
};
const int NUM_DEVICES = 7;
int wearableBattery = -1; // -1 = unknown

void checkDevices() {
    WiFiClient client;
    HTTPClient http;
    http.setReuse(false);
    http.setTimeout(1000); // [Fixed] Increased timeout to 1 second

    for (int i = 0; i < NUM_DEVICES; i++) {
        String url = "http://" + devices[i].ip.toString() + ENDPOINT_HEARTBEAT;
        
        // Special check for AP (Gateway) - Just check wifi connection
        if (devices[i].ip == IP_STATION1_AP) {
            devices[i].online = (WiFi.status() == WL_CONNECTED);
            Serial.printf("Gateway (S1 AP): %s\n", devices[i].online ? "ONLINE" : "OFFLINE");
            continue;
        }

        // [Fixed] Proper HTTP connection handling
        if (http.begin(client, url)) {
            int code = http.GET();
            
            if (code == 200) {
                devices[i].online = true;
                Serial.printf("%s: ONLINE\n", devices[i].name.c_str());
            } else {
                devices[i].online = false;
                Serial.printf("%s: OFFLINE (code: %d)\n", devices[i].name.c_str(), code);
            }
            
            http.end();
            client.stop(); // [Added] Properly close connection
        } else {
            devices[i].online = false;
            Serial.printf("%s: OFFLINE (connection failed)\n", devices[i].name.c_str());
        }
        
        delay(50); // [Added] Small delay between requests
    }
    Serial.println("---");
}

void updateWearableBattery() {
    if (WiFi.status() != WL_CONNECTED) {
        wearableBattery = -1;
        return;
    }

    WiFiClient client;
    HTTPClient http;
    http.setReuse(false);
    http.setTimeout(1000); // [Increased] to 1 second

    String url = "http://" + IP_STICKC.toString() + "/api/battery";
    
    if (http.begin(client, url)) {
        int code = http.GET();
        if (code == 200) {
            String body = http.getString();
            wearableBattery = body.toInt();
            Serial.printf("Wearable Battery: %d%%\n", wearableBattery);
        } else {
            wearableBattery = -1;
            Serial.printf("Wearable Battery: N/A (code: %d)\n", code);
        }
        http.end();
        client.stop(); // [Added]
    } else {
        wearableBattery = -1;
        Serial.println("Wearable Battery: N/A (connection failed)");
    }
}

void drawDashboard() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.print("SYSTEM MONITOR");

    // WiFi Status Indicator
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(250, 5);
    if (WiFi.status() == WL_CONNECTED) {
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.print("WiFi");
    } else {
        M5.Lcd.setTextColor(RED, BLACK);
        M5.Lcd.print("WiFi");
    }

    // Battery info for Wearable (StickCPlus2)
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(200, 15);
    M5.Lcd.setTextColor(CYAN, BLACK);
    M5.Lcd.print("Wearable");

    M5.Lcd.setCursor(200, 27);
    if (wearableBattery >= 0) {
        if (wearableBattery > 20) {
            M5.Lcd.setTextColor(GREEN, BLACK);
        } else {
            M5.Lcd.setTextColor(RED, BLACK);
        }
        M5.Lcd.printf("Bat: %d%%", wearableBattery);
    } else {
        M5.Lcd.setTextColor(RED, BLACK);
        M5.Lcd.print("Bat: N/A");
    }

    M5.Lcd.setTextSize(1);
    int y = 45;
    for (int i = 0; i < NUM_DEVICES; i++) {
        M5.Lcd.setCursor(10, y);
        if (devices[i].online) {
            M5.Lcd.setTextColor(GREEN, BLACK);
            M5.Lcd.printf("[ON]  %s", devices[i].name.c_str());
        } else {
            M5.Lcd.setTextColor(RED, BLACK);
            M5.Lcd.printf("[OFF] %s", devices[i].name.c_str());
        }
        y += 20;
    }

    // Reset Button Prompt
    M5.Lcd.setTextColor(YELLOW, BLACK);
    M5.Lcd.setCursor(40, 210);
    M5.Lcd.print("Btn B: GLOBAL RESET");
}

void triggerGlobalReset() {
    Serial.println("\n>>> GLOBAL RESET TRIGGERED <<<");
    
    M5.Lcd.fillScreen(RED);
    M5.Lcd.setCursor(50, 110);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.print("RESETTING...");
    
    WiFiClient client;
    HTTPClient http;
    http.setReuse(false);
    http.setTimeout(1000); // [Increased]
    
    int resetCount = 0;
    
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (!devices[i].online) {
            Serial.printf("Skipping %s (offline)\n", devices[i].name.c_str());
            continue;
        }
        
        String url = "http://" + devices[i].ip.toString() + ENDPOINT_RESET_GLOBAL;
        
        if (http.begin(client, url)) {
            http.addHeader("Content-Type", "application/json");
            int code = http.POST("{}");
            
            if (code > 0) {
                Serial.printf("Reset %s: SUCCESS (code: %d)\n", devices[i].name.c_str(), code);
                resetCount++;
            } else {
                Serial.printf("Reset %s: FAILED (%s)\n", devices[i].name.c_str(), http.errorToString(code).c_str());
            }
            
            http.end();
            client.stop(); // [Added]
        } else {
            Serial.printf("Reset %s: Connection failed\n", devices[i].name.c_str());
        }
        
        delay(200); // [Added] Delay between reset requests
    }
    
    Serial.printf(">>> Reset Complete: %d/%d devices <<<\n\n", resetCount, NUM_DEVICES);
    
    delay(1000);
    drawDashboard();
}

void setup() {
    M5.begin();
    
    // [Added] Serial Debug
    Serial.begin(115200);
    Serial.println("\n\n=== Reset-Basic Core Monitor Starting ===");
    
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 100);
    M5.Lcd.print("Connecting WiFi...");
    
    // [Fixed] Configure IP before connecting
    WiFi.mode(WIFI_STA);
    WiFi.config(IP_RESET_MON, IP_STATION1_AP, NETMASK, IP_STATION1_AP);
    
    Serial.printf("Connecting to: %s\n", AP_SSID);
    Serial.printf("Target IP: %s\n", IP_RESET_MON.toString().c_str());
    
    WiFi.begin(AP_SSID, AP_PASSWORD);
    
    // [Added] Connection timeout
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 30) {
        delay(500);
        M5.Lcd.print(".");
        Serial.print(".");
        retry++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✓ WiFi Connected");
        Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("  Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("  Subnet: %s\n", WiFi.subnetMask().toString().c_str());
    } else {
        Serial.println("\n✗ WiFi Connection Failed!");
        M5.Lcd.fillScreen(RED);
        M5.Lcd.setTextColor(WHITE);
        M5.Lcd.setCursor(50, 110);
        M5.Lcd.print("WiFi Failed!");
        while(1) delay(1000); // Halt on error
    }
    
    Serial.println("✓ System Ready");
    Serial.println("=== Monitoring Started ===\n");
    
    drawDashboard();
}

void loop() {
    M5.update();
    
    // Check status every 3 seconds (increased from 2)
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 3000) {
        Serial.println("Checking devices...");
        checkDevices();
        updateWearableBattery();
        drawDashboard();
        lastCheck = millis();
    }

    // Reset Trigger
    if (M5.BtnB.wasPressed()) {
        triggerGlobalReset();
    }
}
