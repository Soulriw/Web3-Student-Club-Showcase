#include <M5Atom.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <HTTPClient.h>

// --- CONFIGURATION ---
const char *SSID = "Web3Showcase_AP";
const char *PASSWORD = "12345678";

// IP ของ StickC
const char* STICKC_IP = "192.168.4.2"; 
const int STICKC_PORT = 88;

// MAC Address ของ Atom Echo
uint8_t echoAddress[] = {0x90, 0x15, 0x06, 0xFD, 0xF2, 0xF8}; 

const int RSSI_THRESHOLD = -55;

BLEUUID targetUUID = BLEUUID("1234");
int checkIcon[] = { 15, 21, 17, 13, 9 };

BLEScan* pBLEScan;
bool isVerified = false;

// --- FUNCTIONS ---

void sendAuthStartToStickC() {
    if(WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String myMac = WiFi.macAddress();
        String url = "http://" + String(STICKC_IP) + ":" + String(STICKC_PORT) + "/auth_start?sender_mac=" + myMac;
        
        http.begin(url);
        http.setConnectTimeout(1000);
        int httpCode = http.GET();
        
        if (httpCode > 0) {
            Serial.printf("[HTTP] Sent Auth Start to StickC (Code: %d)\n", httpCode);
        } else {
            Serial.printf("[HTTP] Failed to send to StickC (Error: %s)\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    } else {
        Serial.println("[HTTP] WiFi not connected!");
    }
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    if (len > 0 && *incomingData == 2) {
        isVerified = true;
        Serial.println("[ESP-NOW] Received Confirmation from Echo");
    }
}

void setup() {
    M5.begin(true, false, true); 
    delay(10);
    
    // 1. Connect WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASSWORD);
    
    M5.dis.fillpix(0xFF0000); 
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected!");
    
    // 2. Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    
    esp_now_register_recv_cb(OnDataRecv);

    // Register Echo Peer
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, echoAddress, 6);
    peerInfo.channel = 0; 
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        Serial.println("Failed to add peer");
    }

    // 3. Init BLE
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan(); 
    pBLEScan->setActiveScan(true); 
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    
    M5.dis.fillpix(0x0000FF); 
    Serial.println("System Ready.");
}

void loop() {
    M5.update();
    
    if (isVerified) {
        M5.dis.clear();
        for (int i = 0; i < 5; i++) M5.dis.drawpix(checkIcon[i], 0x00FF00); 
        Serial.println("Task Complete.");
        while(true) { delay(1000); } 
    }

    // 
    BLEScanResults foundDevices = pBLEScan->start(1, false); 
    bool foundTarget = false;
    
    // ใช้ . แทน -> เพราะ foundDevices เป็น Object แล้ว
    for (int i = 0; i < foundDevices.getCount(); i++) {
        BLEAdvertisedDevice device = foundDevices.getDevice(i);
        if (device.haveServiceUUID() && device.isAdvertisingService(targetUUID)) {
             int rssi = device.getRSSI();
             if (rssi > RSSI_THRESHOLD) {
                foundTarget = true;
                Serial.printf("Found StickC! RSSI: %d\n", rssi);
             }
             break;
        }
    }
    
    if (foundTarget) {
        Serial.println("Target in Range!");
        M5.dis.fillpix(0xFFFF00); 

        // A. ส่งเสียงไป Echo
        uint8_t data = 1; 
        esp_now_send(echoAddress, &data, sizeof(data));
        
        // B. ส่ง HTTP ไป StickC
        sendAuthStartToStickC();
        
        delay(500); 
    } 
    else {
        M5.dis.fillpix(0x0000FF); 
    }

    pBLEScan->clearResults(); 
}