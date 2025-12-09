#include <M5Atom.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> 

//ใส่ MAC Address
uint8_t paperAddress[]  = {0x08, 0xF9, 0xE0, 0xF6, 0x23, 0x58}; // M5Paper
uint8_t stickc1Address[] = {0x00, 0x4B, 0x12, 0xC4, 0x2D, 0xF8}; // StickC Plus2
uint8_t stickc2Address[] = {0x00, 0x4B, 0x12, 0xC4, 0x35, 0x48};
uint8_t echoAddress[]   = {0x90, 0x15, 0x06, 0xFA, 0xE7, 0x70}; // Echo

const int INITIAL_BALANCE = 5;

struct MenuItem {
    const char* name;
    int price;
};

const MenuItem menuList[] = {
    {"Invalid", 0},
    {"Coffee", 2},
    {"Croissant", 3},
    {"Lunch Set", 5},
    {"Tea", 2}
};
//try
// โครงสร้างข้อมูล
typedef struct struct_menu_id {
    uint8_t menuID;    
} struct_menu_id;
struct_menu_id incomingMenu;

typedef struct struct_order {
    char menuName[32];
    int price;
    int newBalance;
} struct_order;
struct_order outgoingOrder;

int standbyIcon[] = { 0, 1, 2, 3, 4, 5, 9, 10, 12, 14, 15, 19, 20, 21, 22, 23, 24 };
// ไอคอนติ๊กถูก
int checkIcon[] = { 15, 21, 17, 13, 9 };

void showStandbyPattern() {
    M5.dis.clear();
    for (int i = 0; i < (sizeof(standbyIcon) / sizeof(standbyIcon[0])); i++) M5.dis.drawpix(standbyIcon[i], 0x0000FF);
}

void showSuccessPattern() {
    M5.dis.clear();
    // ต้องครอบการหารทั้งหมดด้วยวงเล็บเดียว (sizeof(array) / sizeof(element))
    for (int i = 0; i < (sizeof(checkIcon) / sizeof(checkIcon[0])); i++){}//สีเขียว
}

void showProcessingPattern() {
    M5.dis.clear();
    M5.dis.fillpix(0xFFFF00); //สีเหลืองทั้งจอ คือ กำลังส่ง
}



void addPeer(uint8_t *macAddr) {
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, macAddr, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    esp_now_del_peer(macAddr);
    esp_now_add_peer(&peerInfo);
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Last Packet Send Status: ");
    if (status == ESP_NOW_SEND_SUCCESS) {
        Serial.println("Delivery Success");
    } else {
        Serial.println("Delivery Fail");

        showStandbyPattern(); 
    }
}
//ฟังก์ชันทำงานเมื่อได้รับข้อมูลกลับมาจากPaper
void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *incomingData, int len) {
    //ตรวจสอบว่าเป็นข้อมูล Order ไหม
    if (len == 1) {
        uint8_t id= incomingData[0];

        if (id >= 1 && id <= 4){
            int cost =menuList[id].price;
            int newBalance = INITIAL_BALANCE -cost;
            
            Serial.printf(" Received Order from Paper: Menu: %s, Price: %d\n", id, menuList[id].name, cost);
             
            if (newBalance >= 0){
                strcpy(outgoingOrder.menuName, menuList[id].name);
                outgoingOrder.price = cost;
                outgoingOrder.newBalance = newBalance;

                Serial.printf("   - New Balance: %d CCoin\n", outgoingOrder.newBalance);

                esp_now_send(stickc1Address, (uint8_t *) &outgoingOrder, sizeof(outgoingOrder));
                esp_now_send(stickc2Address, (uint8_t *) &outgoingOrder, sizeof(outgoingOrder));
        //สั่ง Echo ให้ร้องเตือน
                uint8_t triggerSignal = 1;
                esp_now_send(echoAddress, &triggerSignal, sizeof(triggerSignal));

        //แสดงติ๊กถูกสีเขียว
                showSuccessPattern();
                delay(2000);
                showStandbyPattern();
            
            }else{
                Serial.printf("Insufficient Balance. Need %d, have %d. Transaction rejected.\n", cost, INITIAL_BALANCE);
                M5.dis.fillpix(0xFF0000);
                delay(2000);
                showStandbyPattern();
            }
        }else {
            Serial.printf("Received Invalid data size from MAC: %02X.", info->src_addr[0]);
            M5.dis.fillpix(0xFF00FF);
            delay(1000);
            showStandbyPattern();
        }
    } 
}

void setup() {
    M5.begin(true, false, true);
    delay(10);
    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK){
      Serial.println("ESP-NOW Init Failed! Restarting....");
      delay(2000);
      ESP.restart();
    }

    esp_now_register_recv_cb(OnDataRecv);
    esp_now_register_send_cb(OnDataSent);
    
    addPeer(paperAddress);
    addPeer(stickc1Address);
    addPeer(stickc2Address);
    addPeer(echoAddress);

    Serial.println("Atom-Matrix System Ready");
    showStandbyPattern(); //สีน้ำเงิน
}

void loop() {
    M5.update();

    if (M5.Btn.wasPressed()) {
        
        //แสดงสีเหลือง ส่งข้อมูลไปถาม Paper
        showProcessingPattern(); 

        //ส่ง Requestไปหา Paper  ตอนนี้ส่งแค่สัญญาณเปล่าๆ
        uint8_t requestSignal = 1 ;
        esp_now_send(paperAddress, &requestSignal, sizeof(requestSignal));
        
        Serial.println("Sent Request to Paper(Asking for Order Data)...");
    }
    delay(10);
}
