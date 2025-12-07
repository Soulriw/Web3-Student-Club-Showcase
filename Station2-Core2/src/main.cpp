// Station2_Core2_Monitor.cpp
// Hardware: M5Stack Core2 (Second Unit)
// Function: Display Authentication Status

#include <M5Core2.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "config.h"
#include "Participant.h"

AsyncWebServer server(80);
Participant participant;

void drawMonitor() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextDatum(TC_DATUM);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setFont(&fonts::Font4);
    M5.Lcd.drawString("Station 2: Authentication", 160, 10);
    
    M5.Lcd.setTextDatum(MC_DATUM);
    if (participant.Username == "") {
        M5.Lcd.setTextColor(DARKGREY, BLACK);
        M5.Lcd.drawString("Waiting for User...", 160, 120);
    } else {
        M5.Lcd.setFont(&fonts::lgfxThai_24);
        M5.Lcd.setTextColor(CYAN, BLACK);
        M5.Lcd.drawString("Verifying: " + participant.Username, 160, 90);
        
        if (participant.isAuthenticated) {
            M5.Lcd.setTextColor(GREEN, BLACK);
            M5.Lcd.drawString("☑ AUTH SUCCESS", 160, 140);
        } else {
            M5.Lcd.setTextColor(YELLOW, BLACK);
            M5.Lcd.drawString("... In Progress ...", 160, 140);
        }
    }
}

void setup() {
    M5.begin();
    WiFi.begin(AP_SSID, AP_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    
    // IP Config
    WiFi.config(IP_STATION2_MON, IP_STATION1_AP, NETMASK, IP_STATION1_AP);

    // Handlers
    server.on(ENDPOINT_SET_USER, HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("username", true)) {
            participant.Username = request->getParam("username", true)->value();
            participant.isAuthenticated = false;
            drawMonitor();
            request->send(200);
        } else request->send(400);
    });

    server.on(ENDPOINT_SET_AUTH, HTTP_POST, [](AsyncWebServerRequest *request){
        participant.isAuthenticated = true;
        drawMonitor();
        request->send(200);
    });

    server.on(ENDPOINT_RESET_GLOBAL, HTTP_POST, [](AsyncWebServerRequest *request){
        participant.reset();
        drawMonitor();
        request->send(200);
    });
    
    // Heartbeat Endpoint
    server.on(ENDPOINT_HEARTBEAT, HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "OK");
    });

    server.begin();
    drawMonitor();
}

void loop() {
    M5.update();
}