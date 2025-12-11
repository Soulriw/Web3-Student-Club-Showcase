#include <M5Atom.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> 

//ใส่ MAC Address
uint8_t paperAddress[]  = {0x08, 0xF9, 0xE0, 0xF6, 0x23, 0x58}; // M5Paper
uint8_t stickc1Address[]  = {0x00, 0x4B, 0x12, 0xC4, 0x2D, 0xF8};
uint8_t stickc2Address[]  = {0x00, 0x4b, 0x12, 0xC4, 0x35, 0x48};
uint8_t echoAddress[]   = {0x90, 0x15, 0x06, 0xFA, 0xE7, 0x70}; // Echo

int standbyIcon[] = { 0, 1, 2, 3, 4, 5, 9, 10, 12, 14, 15, 19, 20, 21, 22, 23, 24 };

// ไอคอนติ๊กถูก
int checkIcon[] = { 15, 21, 17, 13, 9 };

bool sending = false;

typedef struct struct_message {
  char msg[32];
} struct_message;

struct_message outgoing;

void showStandbyPattern() {
    M5.dis.clear();
    for (int i = 0; i < 17; i++) M5.dis.drawpix(standbyIcon[i], 0x0000FF);
}

void showSuccessPattern() {
    M5.dis.clear();
    for (int i = 0; i < 5; i++) M5.dis.drawpix(checkIcon[i], 0x00FF00); //สีเขียว
}

void showProcessingPattern() {
    M5.dis.clear();
    M5.dis.fillpix(0xFFFF00); //สีเหลืองทั้งจอ คือ กำลังส่ง
}

void showFailedPattern() {
    M5.dis.clear();
    M5.dis.fillpix(0xFF0000); //สีแดงทั้งจอ คือ ไม่ส่งได้
}

void addPeer(uint8_t *macAddr) {
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, macAddr, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
}

//ฟังก์ชันทำงานเมื่อได้รับข้อมูลกลับมาจากPaper
void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *incomingData, int len) {

    // Print first received byte and match from list
    // 49 = 1st choice
    // 50 = 2nd choice
    // 51 = 3rd choice
    // 51 = 4th choice

    Serial.print("Recieved: ");
    Serial.print(incomingData[0]);
    Serial.println();

    if (incomingData[0] == 48) {
        showFailedPattern();
        delay(2000);
        showStandbyPattern();
        sending = false;
        return;
    } else if (incomingData[0] == 45) {
        Serial.println("Received reset trigger from Core");
        String msg = String(-1);
        strcpy(outgoing.msg, msg.c_str());
        esp_now_send(echoAddress, (uint8_t *)&outgoing, sizeof(outgoing));
        ESP.restart();
    }

    // send data to stick c and handle coin moderation
    esp_now_send(echoAddress, &incomingData[0], sizeof(&incomingData[0]));
    esp_now_send(stickc1Address, &incomingData[0], sizeof(incomingData[0]));
    esp_now_send(stickc2Address, &incomingData[0], sizeof(incomingData[0]));

    showSuccessPattern();
    delay(2000); //โชว์ค้างไว้ 2 วินาที
    showStandbyPattern(); //กลับไปเป็นสีน้ำเงิน
    sending = false;
}

void setup() {
    M5.begin(true, false, true);
    delay(10);
    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) return;
    
    addPeer(paperAddress);
    addPeer(stickc1Address);
    addPeer(stickc2Address);
    addPeer(echoAddress);

    esp_now_register_recv_cb(OnDataRecv);

    Serial.println("System Ready");
    showStandbyPattern(); //สีน้ำเงิน
}

void loop() {
    M5.update();
    if (!sending && M5.Btn.wasPressed()) {
        
        //แสดงสีเหลือง ส่งข้อมูลไปถาม Paper
        showProcessingPattern();

        sending = true;
    } else if (sending) {
        Serial.println("Sent Request to Paper...");
        uint8_t requestSignal = 1 ;
        esp_now_send(paperAddress, &requestSignal, sizeof(requestSignal));
        delay(2000);
    }

    delay(100);
}