#include <M5Core2.h>
#include <WiFi.h>
#include <esp_now.h>

// --- ส่วนประกาศตัวแปรและโครงสร้างข้อมูล ---
typedef struct struct_message {
  char type[10];     
  char username[50];  
  int status;
} struct_message;

struct_message incomingReadings;

volatile bool new_data_received = false; 
volatile bool reset_triggered = false;   

String display_username = "";

// --- ส่วนแสดงผลหน้าจอ ---
void displayResetScreen() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextDatum(MC_DATUM);
  
  M5.Lcd.drawString("SYSTEM STATUS:", 160, 100);
  M5.Lcd.setTextColor(CYAN, BLACK);
  M5.Lcd.drawString("Waiting for Request...", 160, 140);
}

void displayAuthSuccess(String username) {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextSize(3);
  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.drawString("Verifying...", 160, 120);
  
  delay(500); 

  M5.Lcd.fillScreen(GREEN);
  M5.Lcd.setTextColor(BLACK, GREEN);
  M5.Lcd.setTextSize(2);
  
  String verifyText = "Verifying (" + username + ")";
  M5.Lcd.drawString(verifyText, 160, 90);
  
  M5.Lcd.setTextSize(3);
  M5.Lcd.drawString("Authentication", 160, 140);
  M5.Lcd.drawString("Successful", 160, 180);
  
  // Graphic
  M5.Lcd.drawCircle(160, 50, 20, BLACK);
  M5.Lcd.drawLine(150, 50, 160, 60, BLACK);
  M5.Lcd.drawLine(160, 60, 175, 40, BLACK);
}

// --- ส่วนรับข้อมูล ESP-NOW ---
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
  
  // กรณีได้รับคำสั่ง AUTH (ยืนยันตัวตน)
  if (strcmp(incomingReadings.type, "AUTH") == 0 && incomingReadings.status == 1) {
    display_username = String(incomingReadings.username);
    new_data_received = true;
    Serial.println("Command: AUTH RECEIVED");
  }
  
  // กรณีได้รับคำสั่ง RESET (รีเซ็ตระบบ)
  else if (strcmp(incomingReadings.type, "RESET") == 0) {
    reset_triggered = true;
    Serial.println("Command: RESET RECEIVED");
  }
}

// --- Setup ---
void setup() {
  M5.begin();
  Serial.begin(115200);

  displayResetScreen();

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // ลงทะเบียนฟังก์ชันรับข้อมูล (ส่วนที่ขาดไปก่อนหน้านี้)
  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("Station 2 Ready...");
}

// --- Loop (ส่วนที่ขาดไปก่อนหน้านี้) ---
void loop() {
  
  // 1. เช็คคำสั่ง RESET ก่อน (สำคัญสุด)
  if (reset_triggered) {
    displayResetScreen(); 
    reset_triggered = false; 
    new_data_received = false; 
  }

  // 2. เช็คคำสั่ง AUTH
  if (new_data_received) {
    displayAuthSuccess(display_username);
    new_data_received = false;
  }
  
  M5.update();
}
