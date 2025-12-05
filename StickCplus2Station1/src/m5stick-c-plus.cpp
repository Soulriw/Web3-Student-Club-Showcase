/*
 * STATION 1 - Identity Creation (M5Stack Core2)
 * 
 * หน้าที่:
 * - สร้าง WiFi Access Point (SoftAP)
 * - แสดง QR Code ที่มี SSID และข้อมูลการเชื่อมต่อ
 * - รัน WebServer สำหรับ Captive Portal
 * - รับ Username จาก HTTP POST
 * - ส่ง Username ไปยัง StickC-Plus2 ผ่าน HTTP
 */

 #include <M5Core2.h>
 #include <WiFi.h>
 #include <ESPAsyncWebServer.h>
 #include <qrcode.h>
 #include <HTTPClient.h>
 #include <ArduinoJson.h>
 
 // ========== ค่าคงที่และตัวแปร ==========
 const char* AP_SSID = "Web3-Showcase";
 const char* AP_PASSWORD = "web3club2024";
 const IPAddress LOCAL_IP(192, 168, 4, 1);
 const IPAddress GATEWAY(192, 168, 4, 1);
 const IPAddress SUBNET(255, 255, 255, 0);
 
 // StickC-Plus2 IP Address (ต้องตรงกับที่ StickC กำหนด)
 const char* STICKC_IP = "http://192.168.4.100";
 
 AsyncWebServer server(80);
 String currentUsername = "";
 bool newUserRegistered = false;
 
 // ========== HTML สำหรับ Captive Portal ==========
 const char index_html[] PROGMEM = R"rawliteral(
 <!DOCTYPE html>
 <html>
 <head>
     <meta charset="UTF-8">
     <meta name="viewport" content="width=device-width, initial-scale=1.0">
     <title>Web3 Showcase - Identity Creation</title>
     <style>
         * {
             margin: 0;
             padding: 0;
             box-sizing: border-box;
         }
         body {
             font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
             background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
             min-height: 100vh;
             display: flex;
             justify-content: center;
             align-items: center;
             padding: 20px;
         }
         .container {
             background: white;
             padding: 40px 30px;
             border-radius: 20px;
             box-shadow: 0 20px 60px rgba(0,0,0,0.3);
             max-width: 400px;
             width: 100%;
             animation: slideIn 0.5s ease-out;
         }
         @keyframes slideIn {
             from {
                 opacity: 0;
                 transform: translateY(-30px);
             }
             to {
                 opacity: 1;
                 transform: translateY(0);
             }
         }
         h1 {
             color: #667eea;
             font-size: 28px;
             margin-bottom: 10px;
             text-align: center;
         }
         .subtitle {
             color: #666;
             font-size: 14px;
             text-align: center;
             margin-bottom: 30px;
         }
         .form-group {
             margin-bottom: 25px;
         }
         label {
             display: block;
             color: #333;
             font-weight: 600;
             margin-bottom: 8px;
             font-size: 14px;
         }
         input[type="text"] {
             width: 100%;
             padding: 12px 15px;
             border: 2px solid #e0e0e0;
             border-radius: 10px;
             font-size: 16px;
             transition: all 0.3s ease;
         }
         input[type="text"]:focus {
             outline: none;
             border-color: #667eea;
             box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);
         }
         button {
             width: 100%;
             padding: 14px;
             background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
             color: white;
             border: none;
             border-radius: 10px;
             font-size: 16px;
             font-weight: 600;
             cursor: pointer;
             transition: transform 0.2s ease, box-shadow 0.2s ease;
         }
         button:hover {
             transform: translateY(-2px);
             box-shadow: 0 10px 25px rgba(102, 126, 234, 0.4);
         }
         button:active {
             transform: translateY(0);
         }
         .info {
             background: #f0f4ff;
             padding: 15px;
             border-radius: 10px;
             margin-top: 20px;
             font-size: 13px;
             color: #666;
             text-align: center;
         }
         .success-message {
             display: none;
             background: #4caf50;
             color: white;
             padding: 15px;
             border-radius: 10px;
             margin-top: 20px;
             text-align: center;
             font-weight: 600;
         }
         .error-message {
             display: none;
             background: #f44336;
             color: white;
             padding: 15px;
             border-radius: 10px;
             margin-top: 20px;
             text-align: center;
             font-weight: 600;
         }
     </style>
 </head>
 <body>
     <div class="container">
         <h1>🌐 Web3 Showcase</h1>
         <p class="subtitle">Create Your Digital Identity</p>
         
         <form id="usernameForm">
             <div class="form-group">
                 <label for="username">Enter Your Username</label>
                 <input type="text" id="username" name="username" 
                        placeholder="e.g., satoshi_21" 
                        required minlength="3" maxlength="20">
             </div>
             <button type="submit">Create Identity 🚀</button>
         </form>
         
         <div class="success-message" id="successMsg">
             ✓ Identity created successfully!
         </div>
         <div class="error-message" id="errorMsg">
             ✗ Failed to create identity. Please try again.
         </div>
         
         <div class="info">
             💡 Your username will be used across all stations
         </div>
     </div>
 
     <script>
         document.getElementById('usernameForm').addEventListener('submit', async function(e) {
             e.preventDefault();
             
             const username = document.getElementById('username').value.trim();
             const successMsg = document.getElementById('successMsg');
             const errorMsg = document.getElementById('errorMsg');
             const submitBtn = this.querySelector('button');
             
             // Validate username
             if (username.length < 3) {
                 errorMsg.textContent = '✗ Username must be at least 3 characters';
                 errorMsg.style.display = 'block';
                 successMsg.style.display = 'none';
                 return;
             }
             
             // Disable button during submission
             submitBtn.disabled = true;
             submitBtn.textContent = 'Creating...';
             
             try {
                 const response = await fetch('/register', {
                     method: 'POST',
                     headers: {
                         'Content-Type': 'application/json',
                     },
                     body: JSON.stringify({ username: username })
                 });
                 
                 if (response.ok) {
                     successMsg.style.display = 'block';
                     errorMsg.style.display = 'none';
                     submitBtn.textContent = 'Identity Created ✓';
                     
                     // Reset form after 3 seconds
                     setTimeout(() => {
                         document.getElementById('username').value = '';
                         submitBtn.disabled = false;
                         submitBtn.textContent = 'Create Identity 🚀';
                         successMsg.style.display = 'none';
                     }, 3000);
                 } else {
                     throw new Error('Registration failed');
                 }
             } catch (error) {
                 errorMsg.style.display = 'block';
                 successMsg.style.display = 'none';
                 submitBtn.disabled = false;
                 submitBtn.textContent = 'Create Identity 🚀';
             }
         });
     </script>
 </body>
 </html>
 )rawliteral";
 
 // ========== ฟังก์ชันสร้างและแสดง QR Code ==========
 void displayQRCode() {
     M5.Lcd.fillScreen(TFT_WHITE);
     
     // Header
     M5.Lcd.setTextColor(TFT_PURPLE, TFT_WHITE);
     M5.Lcd.setTextSize(2);
     M5.Lcd.setTextDatum(MC_DATUM);
     M5.Lcd.drawString("Web3 Showcase", 160, 20);
     
     M5.Lcd.setTextSize(1);
     M5.Lcd.setTextColor(TFT_DARKGREY, TFT_WHITE);
     M5.Lcd.drawString("Identity Creation Station", 160, 45);
     
     // สร้าง QR Code
     QRCode qrcode;
     uint8_t qrcodeData[qrcode_getBufferSize(5)];
     
     // สร้าง URL สำหรับ Captive Portal
     String qrText = "WIFI:T:WPA;S:" + String(AP_SSID) + ";P:" + String(AP_PASSWORD) + ";;";
     
     qrcode_initText(&qrcode, qrcodeData, 5, 0, qrText.c_str());
     
     // วาด QR Code (ขนาด 4x4 pixels ต่อ module)
     int scale = 4;
     int offsetX = (320 - qrcode.size * scale) / 2;
     int offsetY = 70;
     
     M5.Lcd.fillRect(offsetX - 10, offsetY - 10, 
                     qrcode.size * scale + 20, 
                     qrcode.size * scale + 20, TFT_WHITE);
     
     for (uint8_t y = 0; y < qrcode.size; y++) {
         for (uint8_t x = 0; x < qrcode.size; x++) {
             if (qrcode_getModule(&qrcode, x, y)) {
                 M5.Lcd.fillRect(offsetX + x * scale, 
                                offsetY + y * scale, 
                                scale, scale, TFT_BLACK);
             }
         }
     }
     
     // แสดงข้อมูล WiFi
     M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);
     M5.Lcd.setTextSize(1);
     M5.Lcd.drawString("Scan to Connect", 160, offsetY + qrcode.size * scale + 20);
     
     M5.Lcd.setTextColor(TFT_DARKGREY, TFT_WHITE);
     M5.Lcd.drawString("SSID: " + String(AP_SSID), 160, offsetY + qrcode.size * scale + 40);
     M5.Lcd.drawString("Password: " + String(AP_PASSWORD), 160, offsetY + qrcode.size * scale + 55);
     
     // Footer - Status
     M5.Lcd.fillRect(0, 210, 320, 30, TFT_PURPLE);
     M5.Lcd.setTextColor(TFT_WHITE, TFT_PURPLE);
     M5.Lcd.drawString("Waiting for participants...", 160, 225);
 }
 
 // ========== ฟังก์ชันส่ง Username ไป StickC ==========
 void sendUsernameToStickC(String username) {
     HTTPClient http;
     
     String url = String(STICKC_IP) + "/register";
     http.begin(url);
     http.addHeader("Content-Type", "application/json");
     
     StaticJsonDocument<200> doc;
     doc["username"] = username;
     
     String jsonString;
     serializeJson(doc, jsonString);
     
     int httpCode = http.POST(jsonString);
     
     if (httpCode > 0) {
         Serial.printf("✓ Sent username to StickC: %s (HTTP %d)\n", 
                       username.c_str(), httpCode);
     } else {
         Serial.printf("✗ Failed to send to StickC: %s\n", 
                       http.errorToString(httpCode).c_str());
     }
     
     http.end();
 }
 
 // ========== ฟังก์ชันแสดงหน้าจอเมื่อมี User ใหม่ ==========
 void displayNewUserRegistered(String username) {
     M5.Lcd.fillScreen(TFT_GREEN);
     
     M5.Lcd.setTextColor(TFT_WHITE, TFT_GREEN);
     M5.Lcd.setTextSize(2);
     M5.Lcd.setTextDatum(MC_DATUM);
     M5.Lcd.drawString("Identity Created!", 160, 80);
     
     M5.Lcd.setTextSize(3);
     M5.Lcd.drawString(username, 160, 120);
     
     M5.Lcd.setTextSize(1);
     M5.Lcd.drawString("Welcome to Web3 Showcase", 160, 160);
     
     delay(3000);
     displayQRCode();
 }
 
 // ========== Setup ==========
 void setup() {
     M5.begin();
     Serial.begin(115200);
     
     M5.Lcd.fillScreen(TFT_BLACK);
     M5.Lcd.setTextColor(TFT_WHITE);
     M5.Lcd.setTextSize(2);
     M5.Lcd.setTextDatum(MC_DATUM);
     M5.Lcd.drawString("Initializing...", 160, 120);
     
     // สร้าง WiFi Access Point
     WiFi.mode(WIFI_AP);
     WiFi.softAPConfig(LOCAL_IP, GATEWAY, SUBNET);
     WiFi.softAP(AP_SSID, AP_PASSWORD);
     
     Serial.println("\n========================================");
     Serial.println("STATION 1 - Identity Creation (Core2)");
     Serial.println("========================================");
     Serial.printf("AP SSID: %s\n", AP_SSID);
     Serial.printf("AP Password: %s\n", AP_PASSWORD);
     Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
     Serial.println("========================================\n");
     
     // ตั้งค่า Web Server Routes
     
     // Route: หน้าแรก (Captive Portal)
     server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
         request->send_P(200, "text/html", index_html);
     });
     
     // Route: รับ Username จาก Form
     server.on("/register", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
         [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
             
             StaticJsonDocument<200> doc;
             DeserializationError error = deserializeJson(doc, data);
             
             if (error) {
                 request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                 return;
             }
             
             String username = doc["username"].as<String>();
             username.trim();
             
             if (username.length() < 3 || username.length() > 20) {
                 request->send(400, "application/json", 
                             "{\"error\":\"Username must be 3-20 characters\"}");
                 return;
             }
             
             Serial.printf("\n✓ New user registered: %s\n", username.c_str());
             
             currentUsername = username;
             newUserRegistered = true;
             
             // ส่ง Username ไป StickC
             sendUsernameToStickC(username);
             
             request->send(200, "application/json", 
                          "{\"status\":\"success\",\"username\":\"" + username + "\"}");
         });
     
     // Route: Captive Portal Detection
     server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request){
         request->redirect("/");
     });
     
     server.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest *request){
         request->redirect("/");
     });
     
     // 404 Handler
     server.onNotFound([](AsyncWebServerRequest *request){
         request->redirect("/");
     });
     
     server.begin();
     Serial.println("✓ Web Server started");
     
     // แสดง QR Code
     displayQRCode();
 }
 
 // ========== Loop ==========
 void loop() {
     M5.update();
     
     // ตรวจสอบว่ามี User ใหม่ลงทะเบียนหรือไม่
     if (newUserRegistered) {
         displayNewUserRegistered(currentUsername);
         newUserRegistered = false;
     }
     
     // แสดงจำนวน Client ที่เชื่อมต่อ
     static unsigned long lastUpdate = 0;
     if (millis() - lastUpdate > 5000) {
         int clientCount = WiFi.softAPgetStationNum();
         Serial.printf("Connected clients: %d\n", clientCount);
         lastUpdate = millis();
     }
     
     delay(10);
 }
