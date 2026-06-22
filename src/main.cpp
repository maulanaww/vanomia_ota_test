// TESTING DEVICE — OLED + 8 LED + 4 Button + OTA Ready
// Fix: OTA via HTTPS GitHub menggunakan WiFiClientSecure

#include <WiFi.h>
#include <WiFiClientSecure.h>    // ← TAMBAH INI
#include <PubSubClient.h>
#include <HTTPUpdate.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ══════════════════════════════════════════════════════════════
// KONFIGURASI
// ══════════════════════════════════════════════════════════════
const char* WIFI_SSID     = "PIS - IT";
const char* WIFI_PASSWORD = "ITDevInfra24";

const char* MQTT_SERVER    = "mqtt.permataindonesia.com";
const int   MQTT_PORT      = 1838;
const char* MQTT_USERNAME  = "superAdmNyamuk1";
const char* MQTT_PASSWORD  = "hYqS9+*zDTxYN3bQSTPzistq";
const char* MQTT_CLIENT_ID = "ESP32_Testing_Block01";
const char* MQTT_OTA_TOPIC = "ota/block01/testing";

#define FIRMWARE_VERSION "v1.0.0"
#define BUILD_DATE       "2026-06-22"

// ══════════════════════════════════════════════════════════════
// HARDWARE PINS
// ══════════════════════════════════════════════════════════════
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_MOSI     23
#define OLED_CLK      18
#define OLED_DC       21
#define OLED_CS       22
#define OLED_RESET    15

#define BTN_UP   33
#define BTN_DOWN 27
#define BTN_BACK 26
#define BTN_OK   25

const int ledPins[8] = {4, 5, 12, 13, 14, 16, 17, 32};

// ══════════════════════════════════════════════════════════════
// GLOBAL VARIABLES
// ══════════════════════════════════════════════════════════════
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC, OLED_RESET, OLED_CS);

// MQTT pakai plain WiFiClient (MQTT broker kamu non-SSL)
WiFiClient     espClient;
PubSubClient   mqttClient(espClient);

// OTA pakai WiFiClientSecure (GitHub = HTTPS)
WiFiClientSecure secureClient;

enum Page { HOME, MENU_INFO, MENU_LED_TEST };
Page currentPage = HOME;

int  cursorPosition   = 0;
bool wifiConnected    = false;
unsigned long lastButtonPress  = 0;
unsigned long lastLedTestStep  = 0;
int  currentLedTest   = -1;
bool ledTestRunning   = false;

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
// WiFi & MQTT
// ══════════════════════════════════════════════════════════════
void setupWiFi() {
  Serial.print("[WiFi] Connecting to " + String(WIFI_SSID));
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
  mqttClient.setKeepAlive(60);
}

void reconnectMQTT() {
  if (mqttClient.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.print("[MQTT] Connecting...");
  if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.println(" connected!");
    mqttClient.subscribe(MQTT_OTA_TOPIC);
    Serial.println("[MQTT] Subscribed: " + String(MQTT_OTA_TOPIC));
  } else {
    Serial.printf(" failed (rc=%d), retry in 5s\n", mqttClient.state());
    delay(5000);
  }
}

// ══════════════════════════════════════════════════════════════
// OTA — KUNCI FIX ADA DI SINI
// ══════════════════════════════════════════════════════════════
void handleOTAUpdate(String firmwareURL) {
  Serial.println("\n========================================");
  Serial.println("  OTA UPDATE STARTED");
  Serial.println("  URL: " + firmwareURL);
  Serial.println("========================================");

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println("OTA UPDATE");
  display.println("Downloading...");
  display.display();

  // ── FIX UTAMA ──────────────────────────────────────────────
  // GitHub releases pakai HTTPS. WiFiClientSecure diperlukan.
  // setInsecure() = skip verifikasi sertifikat (OK untuk OTA).
  // Kalau mau lebih aman, bisa pasang root CA GitHub.
  // ──────────────────────────────────────────────────────────
  secureClient.setInsecure();

  // Follow redirect wajib — GitHub redirect ke CDN (objects.githubusercontent.com)
  httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  httpUpdate.rebootOnUpdate(false);  // Manual restart supaya bisa log error

  Serial.println("[OTA] Downloading firmware via HTTPS...");
  t_httpUpdate_return ret = httpUpdate.update(secureClient, firmwareURL);

  switch (ret) {
    case HTTP_UPDATE_FAILED: {
      int errCode   = httpUpdate.getLastError();
      String errMsg = httpUpdate.getLastErrorString();
      Serial.printf("[OTA] FAILED! Error (%d): %s\n", errCode, errMsg.c_str());

      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 5);
      display.println("OTA FAILED!");
      display.printf("Err: %d\n", errCode);
      display.println(errMsg.substring(0, 20));  // Truncate biar muat OLED
      display.display();
      delay(5000);
      break;
    }

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
      ESP.restart();
      break;
  }

  Serial.println("========================================\n");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  String topicStr = String(topic);
  Serial.println("[MQTT] Topic  : " + topicStr);
  Serial.println("[MQTT] Payload: " + msg);

  if (topicStr == MQTT_OTA_TOPIC) {
    Serial.println("[OTA] Trigger received!");
    handleOTAUpdate(msg);
  }
}

// ══════════════════════════════════════════════════════════════
// LED CONTROL
// ══════════════════════════════════════════════════════════════
void runLedTest() {
  if (!ledTestRunning) return;
  unsigned long now = millis();
  if (now - lastLedTestStep < 300) return;
  lastLedTestStep = now;

  if (currentLedTest >= 0) ledcWrite(currentLedTest, 0);
  currentLedTest++;
  if (currentLedTest >= 8) currentLedTest = 0;
  ledcWrite(currentLedTest, 255);
  Serial.printf("[LED TEST] LED %d ON\n", currentLedTest + 1);
}

void runLedTest() {
  if (!ledTestRunning) return;
  unsigned long now = millis();
  if (now - lastLedTestStep < 300) return;
  lastLedTestStep = now;

  if (currentLedTest >= 0) ledcWrite(ledPins[currentLedTest], 0);
  currentLedTest++;
  if (currentLedTest >= 8) currentLedTest = 0;
  ledcWrite(ledPins[currentLedTest], 255);
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
  display.setCursor(0, 28);
  display.print("Version: ");
  display.println(FIRMWARE_VERSION);
  display.setCursor(0, 38);
  display.print("Build: ");
  display.println(BUILD_DATE);
  display.setCursor(0, 48);
  display.print("OTA:");
  display.setCursor(0, 56);
  display.println(MQTT_OTA_TOPIC);
}

void drawLedTestPage() {
  display.setTextSize(1);
  display.setCursor(0, 15);
  display.println("LED Test Mode:");
  if (ledTestRunning) {
    display.setCursor(0, 28);
    display.print("Running... LED: ");
    display.print(currentLedTest + 1);
    display.setCursor(0, 48);
    display.print("OK = STOP");
  } else {
    display.setCursor(0, 28);
    display.print("OK = START");
  }
}

void drawUI() {
  display.clearDisplay();
  drawStatusBar();
  if      (currentPage == HOME)          drawHomePage();
  else if (currentPage == MENU_INFO)     drawInfoPage();
  else if (currentPage == MENU_LED_TEST) drawLedTestPage();
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
    if (cursorPosition > 0) cursorPosition--;
    Serial.println("[BTN] UP");
  } else if (digitalRead(BTN_DOWN) == LOW) {
    lastButtonPress = now;
    if (currentPage == HOME && cursorPosition < 1) cursorPosition++;
    Serial.println("[BTN] DOWN");
  } else if (digitalRead(BTN_BACK) == LOW) {
    lastButtonPress = now;
    if (currentPage != HOME) {
      currentPage = HOME;
      cursorPosition = 0;
      if (ledTestRunning) { ledTestRunning = false; allLedsOff(); }
    }
    Serial.println("[BTN] BACK");
  } else if (digitalRead(BTN_OK) == LOW) {
    lastButtonPress = now;
    if (currentPage == HOME) {
      currentPage = (cursorPosition == 0) ? MENU_INFO : MENU_LED_TEST;
    } else if (currentPage == MENU_LED_TEST) {
      ledTestRunning = !ledTestRunning;
      if (ledTestRunning) { currentLedTest = -1; lastLedTestStep = 0; }
      else allLedsOff();
    }
    Serial.println("[BTN] OK");
  }
}

// ══════════════════════════════════════════════════════════════
// SETUP
// ══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);

  Serial.println("\n================================");
  Serial.println("Device: Testing Device Block01");
  Serial.println("Version: " + String(FIRMWARE_VERSION));
  Serial.println("Build: " + String(BUILD_DATE));
  Serial.println("================================\n");

  pinMode(BTN_UP,   INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  pinMode(BTN_OK,   INPUT_PULLUP);

  for (int i = 0; i < 8; i++) {
    ledcSetup(i, 5000, 8);
    ledcAttachPin(ledPins[i], i);
    ledcWrite(i, 0);
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println("[OLED] FAILED!");
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

  // Boot LED animation
  for (int i = 0; i < 8; i++) {
    ledcWrite(ledPins[i], 255);
    delay(80);
    ledcWrite(ledPins[i], 0);
  }

  Serial.println("\n[READY] Waiting for OTA on: " + String(MQTT_OTA_TOPIC));
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
