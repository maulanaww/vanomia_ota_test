// TESTING DEVICE — OLED + 8 LED + 4 Button + OTA Ready
// Semua kode dalam 1 file, tidak butuh file lain
// FIXED: Pakai LEDC API lama untuk kompatibilitas ESP32 Core v2.x

#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPUpdate.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ══════════════════════════════════════════════════════════════
// KONFIGURASI
// ══════════════════════════════════════════════════════════════

// WiFi
const char* WIFI_SSID     = "PIS - IT";
const char* WIFI_PASSWORD = "ITDevInfra24";

// MQTT
const char* MQTT_SERVER    = "mqtt.permataindonesia.com";
const int   MQTT_PORT      = 1838;
const char* MQTT_USERNAME  = "superAdmNyamuk1";
const char* MQTT_PASSWORD  = "hYqS9+*zDTxYN3bQSTPzistq";
const char* MQTT_CLIENT_ID = "ESP32_OTA_Test";
const char* MQTT_OTA_TOPIC = "ota/test/device";

// ══════════════════════════════════════════════════════════════
// HARDWARE PINS
// ══════════════════════════════════════════════════════════════

// OLED (SPI)
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_MOSI     23
#define OLED_CLK      18
#define OLED_DC       21
#define OLED_CS       22
#define OLED_RESET    15

// Buttons
#define BTN_UP        33
#define BTN_DOWN      27
#define BTN_BACK      26
#define BTN_OK        25

// LEDs
const int ledPins[8] = {4, 5, 12, 13, 14, 16, 17, 32};

// ══════════════════════════════════════════════════════════════
// GLOBAL VARIABLES
// ══════════════════════════════════════════════════════════════

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC, OLED_RESET, OLED_CS);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

enum Page { HOME, MENU_INFO, MENU_LED_TEST };
Page currentPage = HOME;

int cursorPosition = 0;
bool wifiConnected = false;
unsigned long lastButtonPress = 0;
unsigned long lastLedTestStep = 0;
int currentLedTest = -1;
bool ledTestRunning = false;

// ══════════════════════════════════════════════════════════════
// FORWARD DECLARATIONS
// ══════════════════════════════════════════════════════════════

void setupWiFi();
void setupMQTT();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void handleOTAUpdate(String firmwareURL);
void drawUI();
void drawStatusBar();
void drawHomePage();
void drawInfoPage();
void drawLedTestPage();
void handleButtons();
void runLedTest();
void allLedsOff();

// ══════════════════════════════════════════════════════════════
// WiFi & MQTT SETUP
// ══════════════════════════════════════════════════════════════

void setupWiFi() {
  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\n[WiFi] Connected — IP: " + WiFi.localIP().toString());
  } else {
    wifiConnected = false;
    Serial.println("\n[WiFi] Connection failed!");
  }
}

void setupMQTT() {
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);
  mqttClient.setSocketTimeout(5);
  mqttClient.setKeepAlive(60);
}

void reconnectMQTT() {
  if (mqttClient.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;
  
  Serial.print("[MQTT] Connecting...");
  
  bool ok = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD);
  
  if (ok) {
    Serial.println(" connected!");
    mqttClient.subscribe(MQTT_OTA_TOPIC);
    Serial.print("[MQTT] Subscribed OTA topic: ");
    Serial.println(MQTT_OTA_TOPIC);
  } else {
    Serial.printf(" failed (rc=%d), retry in 5s\n", mqttClient.state());
    delay(5000);
  }
}

// ══════════════════════════════════════════════════════════════
// OTA HANDLER
// ══════════════════════════════════════════════════════════════

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  
  String topicStr = String(topic);
  Serial.println("\n[MQTT] Topic  : " + topicStr);
  Serial.println("[MQTT] Payload: " + msg);
  
  if (topicStr == MQTT_OTA_TOPIC) {
    Serial.println("\n[OTA] Trigger received!");
    Serial.println("[OTA] Firmware URL: " + msg);
    handleOTAUpdate(msg);
  }
}

void handleOTAUpdate(String firmwareURL) {
  Serial.println("\n========================================");
  Serial.println("  OTA UPDATE STARTED");
  Serial.println("========================================");
  Serial.println("Firmware URL: " + firmwareURL);
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println("OTA UPDATE");
  display.println("Downloading...");
  display.display();
  
  httpUpdate.rebootOnUpdate(true);
  
  Serial.println("Downloading firmware...");
  t_httpUpdate_return ret = httpUpdate.update(espClient, firmwareURL);
  
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("[OTA] FAILED! Error (%d): %s\n", 
                    httpUpdate.getLastError(), 
                    httpUpdate.getLastErrorString().c_str());
      
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 10);
      display.println("OTA FAILED!");
      display.setCursor(0, 25);
      display.println(httpUpdate.getLastErrorString().c_str());
      display.display();
      delay(3000);
      break;
      
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("[OTA] No updates available");
      break;
      
    case HTTP_UPDATE_OK:
      Serial.println("[OTA] SUCCESS! Rebooting...");
      
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 10);
      display.println("OTA SUCCESS!");
      display.setCursor(0, 25);
      display.println("Rebooting...");
      display.display();
      delay(1000);
      break;
  }
  
  Serial.println("========================================\n");
}

// ══════════════════════════════════════════════════════════════
// LED CONTROL — FIXED: pakai channel, bukan pin
// ══════════════════════════════════════════════════════════════

void allLedsOff() {
  for (int i = 0; i < 8; i++) {
    ledcWrite(i, 0);  // ← FIXED: pakai channel index, bukan pin
  }
}

void runLedTest() {
  if (!ledTestRunning) return;
  
  unsigned long now = millis();
  if (now - lastLedTestStep < 300) return;
  
  lastLedTestStep = now;
  
  if (currentLedTest >= 0) {
    ledcWrite(currentLedTest, 0);  // ← FIXED
  }
  
  currentLedTest++;
  if (currentLedTest >= 8) {
    currentLedTest = 0;
  }
  
  ledcWrite(currentLedTest, 255);  // ← FIXED
  
  Serial.printf("[LED TEST] LED %d ON\n", currentLedTest + 1);
}

// ══════════════════════════════════════════════════════════════
// UI DRAWING
// ══════════════════════════════════════════════════════════════

void drawStatusBar() {
  display.fillRect(0, 0, SCREEN_WIDTH, 12, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(2, 2);
  display.print(wifiConnected ? "ONLINE" : "OFFLINE");
  
  display.setCursor(SCREEN_WIDTH - 40, 2);
  display.print("TEST");
  
  display.setTextColor(SSD1306_WHITE);
}

void drawHomePage() {
  display.setTextSize(2);
  display.setCursor(10, 20);
  display.println("TESTING");
  display.setCursor(25, 40);
  display.println("DEVICE");
  
  display.setTextSize(1);
  display.setCursor(10, 56);
  display.print("Press OK for menu");
}

void drawInfoPage() {
  display.setTextSize(1);
  display.setCursor(0, 15);
  display.println("Firmware Info:");
  display.println("");
  
  display.setCursor(0, 28);
  display.print("Version: ");
  display.println(FIRMWARE_VERSION);
  
  display.setCursor(0, 38);
  display.print("Build: ");
  display.println(BUILD_DATE);
  
  display.setCursor(0, 48);
  display.print("OTA Topic:");
  display.setCursor(0, 56);
  display.println(MQTT_OTA_TOPIC);
}

void drawLedTestPage() {
  display.setTextSize(1);
  display.setCursor(0, 15);
  display.println("LED Test Mode:");
  display.println("");
  
  if (ledTestRunning) {
    display.setCursor(0, 28);
    display.print("Running...");
    display.setCursor(0, 38);
    display.print("Current LED: ");
    display.print(currentLedTest + 1);
    display.setCursor(0, 48);
    display.print("Press OK to STOP");
  } else {
    display.setCursor(0, 28);
    display.print("Press OK to START");
    display.setCursor(0, 38);
    display.print("LED will blink");
    display.setCursor(0, 48);
    display.print("sequentially");
  }
}

void drawUI() {
  display.clearDisplay();
  drawStatusBar();
  
  if (currentPage == HOME) {
    drawHomePage();
  } else if (currentPage == MENU_INFO) {
    drawInfoPage();
  } else if (currentPage == MENU_LED_TEST) {
    drawLedTestPage();
  }
  
  display.display();
}

// ══════════════════════════════════════════════════════════════
// BUTTON HANDLING
// ══════════════════════════════════════════════════════════════

void handleButtons() {
  unsigned long now = millis();
  if (now - lastButtonPress < 200) return;
  
  if (digitalRead(BTN_UP) == LOW) {
    lastButtonPress = now;
    if (currentPage == MENU_INFO || currentPage == MENU_LED_TEST) {
      if (cursorPosition > 0) cursorPosition--;
    }
    Serial.println("[BTN] UP");
  }
  else if (digitalRead(BTN_DOWN) == LOW) {
    lastButtonPress = now;
    if (currentPage == HOME) {
      if (cursorPosition < 1) cursorPosition++;
    }
    Serial.println("[BTN] DOWN");
  }
  else if (digitalRead(BTN_BACK) == LOW) {
    lastButtonPress = now;
    if (currentPage != HOME) {
      currentPage = HOME;
      cursorPosition = 0;
      if (ledTestRunning) {
        ledTestRunning = false;
        allLedsOff();
        Serial.println("[LED TEST] Stopped");
      }
    }
    Serial.println("[BTN] BACK");
  }
  else if (digitalRead(BTN_OK) == LOW) {
    lastButtonPress = now;
    
    if (currentPage == HOME) {
      if (cursorPosition == 0) {
        currentPage = MENU_INFO;
        Serial.println("[MENU] Info");
      } else if (cursorPosition == 1) {
        currentPage = MENU_LED_TEST;
        Serial.println("[MENU] LED Test");
      }
    }
    else if (currentPage == MENU_LED_TEST) {
      ledTestRunning = !ledTestRunning;
      if (ledTestRunning) {
        currentLedTest = -1;
        lastLedTestStep = 0;
        Serial.println("[LED TEST] Started");
      } else {
        allLedsOff();
        Serial.println("[LED TEST] Stopped");
      }
    }
  }
}

// ══════════════════════════════════════════════════════════════
// SETUP
// ══════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  
  Serial.println("\n================================");
  Serial.println("Device: OTA Testing");
  Serial.print("Version: ");
  Serial.println(FIRMWARE_VERSION);
  Serial.print("Build: ");
  Serial.println(BUILD_DATE);
  Serial.println("================================\n");
  
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  
  // ══════════════════════════════════════════════════════════
  // FIXED: Setup LEDs pakai API lama (kompatibel ESP32 Core v2.x)
  // ══════════════════════════════════════════════════════════
  for (int i = 0; i < 8; i++) {
    ledcSetup(i, 5000, 8);           // Setup channel i, freq 5kHz, 8-bit
    ledcAttachPin(ledPins[i], i);    // Attach pin ke channel i
    ledcWrite(i, 0);                 // Tulis duty cycle ke channel i
  }
  
  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println("[OLED] FAILED! Check wiring.");
    for (;;) delay(1000);
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(10, 15);
  display.println("TESTING");
  display.setCursor(25, 35);
  display.println("DEVICE");
  display.setTextSize(1);
  display.setCursor(20, 55);
  display.println(FIRMWARE_VERSION);
  display.display();
  delay(2000);
  
  setupWiFi();
  setupMQTT();
  
  Serial.println("[BOOT] LED animation...");
  for (int i = 0; i < 8; i++) {
    ledcWrite(i, 255);  // ← FIXED
    delay(100);
    ledcWrite(i, 0);    // ← FIXED
  }
  
  Serial.println("\n════════════════════════════════════════════");
  Serial.println("  Testing Device Ready!");
  Serial.println("  OLED: 128x64");
  Serial.println("  LEDs: 8");
  Serial.println("  Buttons: 4 (UP/DOWN/BACK/OK)");
  Serial.print("  OTA Topic: ");
  Serial.println(MQTT_OTA_TOPIC);
  Serial.println("════════════════════════════════════════════\n");
}

// ══════════════════════════════════════════════════════════════
// LOOP
// ══════════════════════════════════════════════════════════════

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    static unsigned long lastWifiCheck = 0;
    if (millis() - lastWifiCheck > 15000) {
      lastWifiCheck = millis();
      Serial.println("[WiFi] Disconnected, reconnecting...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  } else {
    wifiConnected = true;
  }
  
  reconnectMQTT();
  mqttClient.loop();
  
  handleButtons();
  runLedTest();
  drawUI();
  
  delay(50);
}
