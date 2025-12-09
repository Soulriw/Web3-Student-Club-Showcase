#include <M5Unified.h> 
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

// --- CONFIGURATION: MAC ADDRESSES ---
uint8_t matrixAddress[] = {0x4C, 0x75, 0x25, 0xAD, 0xB5, 0xBC};
uint8_t core2Address[] = {0x2c, 0xBC, 0xBB, 0x82, 0x91, 0xA8}; 
uint8_t stickc1Address[] = {0x00, 0x4B, 0x12, 0xC4, 0x2D, 0xF8};
uint8_t stickc2Address[] = {0x00, 0x4b, 0x12, 0xC4, 0x35, 0x48};

typedef struct struct_message {
    char type[10];
    char username[50]; 
    int status;
} struct_message;

// ตัวแปร Global สำหรับเก็บข้อมูลที่ได้รับ
struct_message incomingDataBuffer; 

// --- SOUND SEQUENCER & STATE MACHINE VARIABLES ---
const int shortBeepDuration = 200;
const int longBeepDuration = 700;
const int beepFreq = 1319; // ความถี่เสียง
const int pauseDuration = 100; // ช่วงพักระหว่างเสียง

enum SoundState { 
    IDLE, BEEP1_START, BEEP1_WAIT, BEEP1_PAUSE,
    BEEP2_START, BEEP2_WAIT, BEEP2_PAUSE,
    BEEP3_START, BEEP3_WAIT, DONE 
};
SoundState currentSoundState = IDLE;
unsigned long stateChangeTime = 0;

// ------------------------------------
// 📤 ESP-NOW FUNCTIONS (SENDER)
// ------------------------------------

void sendRequestToAll(const char* type, const char* username, int status) {
    struct_message msg;
    strcpy(msg.type, type);
    strcpy(msg.username, username);
    msg.status = status;

    // ส่งไปยัง Core2, StickC1, StickC2
    esp_now_send(core2Address, (uint8_t *) &msg, sizeof(msg));
    esp_now_send(stickc1Address, (uint8_t *) &msg, sizeof(msg));
    esp_now_send(stickc2Address, (uint8_t *) &msg, sizeof(msg));

    // ส่งสัญญาณยืนยันกลับไปยัง Matrix
    uint8_t matrixData = 3;
    esp_now_send(matrixAddress, &matrixData, sizeof(matrixData));

    Serial.println("[ESP-NOW] Sent Request to all devices.");
}

// ------------------------------------
// 📥 ESP-NOW FUNCTIONS (RECEIVER)
// ------------------------------------

// **แก้ไข:** เปลี่ยนชื่อพารามิเตอร์ `incomingData` เป็น `dataPtr` เพื่อไม่ให้ชนกับตัวแปร Global
void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *dataPtr, int len) {
    if(len == sizeof(struct_message)){
        // **แก้ไข:** 'memcy' เป็น 'memcpy' และใช้ตัวแปร Global 'incomingDataBuffer'
        memcpy(&incomingDataBuffer, dataPtr, sizeof(struct_message));

        Serial.print("\n[ESP-NOW] Received Request from: ");
        for(int i=0; i<6; i++){
            Serial.printf("%02X:", info->src_addr[i]);
        }
        Serial.println();
        // **แก้ไข:** ใช้ incomingDataBuffer แทน incomingData
        Serial.printf("Type: %s, User: %s, Status: %d\n", incomingDataBuffer.type, incomingDataBuffer.username, incomingDataBuffer.status);

        // ตรวจสอบเงื่อนไขเพื่อเริ่มกระบวนการเสียง
        if (strcmp(incomingDataBuffer.type, "TRIGGER") == 0 && currentSoundState == IDLE) {
            currentSoundState = BEEP1_START; 
            Serial.println("Sound Sequence Triggered by Matrix.");
        }
    }
}

// ------------------------------------
// SETUP (รวม peer info และลบโค้ดซ้ำ)
// ------------------------------------

void setup() {
    auto cfg = M5.config();
    cfg.output_power = true; 
    M5.begin(cfg); 
    
    M5.Speaker.begin();
    M5.Speaker.setVolume(200); 

    Serial.begin(115200);

    M5.Display.fillScreen(0x0000FF); // ตั้งค่าสีเริ่มต้นเป็นสีน้ำเงิน
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    Serial.println("\n--------------------------------");
    Serial.print("MY MAC ADDRESS: ");
    Serial.println(WiFi.macAddress());
    Serial.println("--------------------------------");

    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE); 

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESR-NOW Init Failed");
        delay(2000);
        ESP.restart();
    }

    esp_now_register_recv_cb(OnDataRecv); 

    // Helper function to add peers
    auto addPeer = [](const uint8_t* addr, uint8_t channel) {
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, addr, 6);
        peerInfo.channel = channel; 
        peerInfo.encrypt = false;
        if (esp_now_add_peer(&peerInfo) != ESP_OK){
            Serial.printf("Failed to add peer: %02X:%02X:%02X:%02X:%02X:%02X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
        }
    };
    
    // ตั้งค่า Peer ทั้งหมด (รวม Core2, Matrix, StickC1, StickC2)
    addPeer(core2Address, 1);
    addPeer(matrixAddress, 1);
    addPeer(stickc1Address, 1);
    addPeer(stickc2Address, 1);

    Serial.println("Atom-Echo Setup Complete. Waiting for requests from Matrix...");
}

// ------------------------------------
// LOOP (รวม Logic ทั้งหมดและลบโค้ดซ้ำ)
// ------------------------------------

void loop() {
    M5.update(); 
    unsigned long currentTime = millis();
    
    // 1. State Machine สำหรับการเล่นเสียงและส่ง Request (Non-blocking)
    if (currentSoundState != IDLE) {
        
        switch (currentSoundState) {
            case BEEP1_START:
                M5.Display.fillScreen(0xFFFF00); // สีเหลือง
                M5.Speaker.tone(beepFreq, shortBeepDuration);
                stateChangeTime = currentTime;
                currentSoundState = BEEP1_WAIT;
                break;

            case BEEP1_WAIT:
                if (currentTime - stateChangeTime >= shortBeepDuration) {
                    stateChangeTime = currentTime;
                    currentSoundState = BEEP1_PAUSE;
                }
                break;
            case BEEP1_PAUSE:
                if (currentTime - stateChangeTime >= pauseDuration) {
                    currentSoundState = BEEP2_START;
                }
                break;
            
            // --- BEEP 2 (เสียงสั้น) ---
            case BEEP2_START:
                M5.Speaker.tone(beepFreq, shortBeepDuration);
                stateChangeTime = currentTime;
                currentSoundState = BEEP2_WAIT;
                break;
            case BEEP2_WAIT:
                if (currentTime - stateChangeTime >= shortBeepDuration) {
                    stateChangeTime = currentTime;
                    currentSoundState = BEEP2_PAUSE;
                }
                break;
            case BEEP2_PAUSE:
                if (currentTime - stateChangeTime >= pauseDuration) {
                    currentSoundState = BEEP3_START;
                }
                break;

            // --- BEEP 3 (เสียงยาว) ---
            case BEEP3_START:
                M5.Speaker.tone(beepFreq, longBeepDuration);
                stateChangeTime = currentTime;
                currentSoundState = BEEP3_WAIT;
                break;
                
            case BEEP3_WAIT:
                if (currentTime - stateChangeTime >= longBeepDuration) {
                    currentSoundState = DONE;
                }
                break;

            case DONE:
                // 2. เมื่อเสียงทั้งหมดเล่นจบ ให้ส่ง Request ต่อไปยังอุปกรณ์อื่นๆ
                M5.Display.fillScreen(0x00FF00); // สีเขียว: ส่ง Request สำเร็จ
                sendRequestToAll("AUTH", "ECHO_ALERT", 1);
                
                // 3. กลับสู่สถานะ IDLE
                currentSoundState = IDLE;
                M5.Speaker.stop(); 
                delay(2000); 
                M5.Display.fillScreen(0x0000FF); // สีน้ำเงิน: กลับไปสถานะรอ
                break;
        }
    }
    
    // 4. Manual Trigger (ใช้ปุ่มเป็นตัวกระตุ้นแทน Matrix)
    // หากต้องการใช้ปุ่มเพื่อทดสอบการทำงาน ให้ใช้โค้ดส่วนนี้
    if (M5.BtnA.wasPressed() && currentSoundState == IDLE) {
        Serial.println("Manual Button Triggered! Starting process.");
        currentSoundState = BEEP1_START; 
    }
}