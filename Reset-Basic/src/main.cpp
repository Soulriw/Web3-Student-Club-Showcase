// Reset_CoreBasic.cpp
// Hardware: M5Stack Core Basic
// Function: System Monitor (Ping all devices) & Global Reset Button

#include <M5Stack.h> // ใช้ Library M5Stack สำหรับ Core Basic
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

void checkDevices() {
    HTTPClient http;
    http.setTimeout(200); // Fast timeout

    for (int i = 0; i < NUM_DEVICES; i++) {
        String url = "http://" + devices[i].ip.toString() + ENDPOINT_HEARTBEAT;
        
        // Skip pinging self if we were in the list, but we aren't
        if (devices[i].ip == IP_STATION1_AP) {
            // Special check for AP (Gateway) - Just check wifi connection
            devices[i].online = (WiFi.status() == WL_CONNECTED);
            continue;
        }

        http.begin(url);
        int code = http.GET();
        http.end();

        if (code == 200) {
            devices[i].online = true;
        } else {
            devices[i].online = false;
        }
    }
}

void drawDashboard() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.print("SYSTEM MONITOR");
    
    M5.Lcd.setTextSize(1);
    int y = 40;
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
    M5.Lcd.fillScreen(RED);
    M5.Lcd.setCursor(50, 110);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.print("RESETTING...");
    
    HTTPClient http;
    http.setTimeout(500);
    
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (!devices[i].online) continue;
        
        String url = "http://" + devices[i].ip.toString() + ENDPOINT_RESET_GLOBAL;
        http.begin(url);
        http.POST("{}");
        http.end();
    }
    
    delay(1000);
    drawDashboard();
}

void setup() {
    M5.begin();
    WiFi.begin(AP_SSID, AP_PASSWORD);
    
    M5.Lcd.print("Connecting to S1...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        M5.Lcd.print(".");
    }
    
    // Static IP for Reset Station
    WiFi.config(IP_RESET_MON, IP_STATION1_AP, NETMASK, IP_STATION1_AP);
    
    drawDashboard();
}

void loop() {
    M5.update();
    
    // Check status every 2 seconds
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 2000) {
        checkDevices();
        drawDashboard();
        lastCheck = millis();
    }

    // Reset Trigger
    if (M5.BtnB.wasPressed()) {
        triggerGlobalReset();
    }
}