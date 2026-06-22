// TESTING OTA — OLED ONLY
// Fokus test OTA dulu, tanpa LED, tanpa button
// Kalau OTA sukses, baru tambah fitur lain

#include <WiFi.h>
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
const char* MQTT_CLIENT_ID = "ESP32_OTA_Test";
const char* MQTT_OTA_TOPIC = "ota/test/device";

#define FIRMWARE_VERSION "v1.0.1"
#define BUILD_DATE "2026-06-22"

// ══════════════════════════════════════════════════════════════
// OLED PINS (SPI)
// ══════════════════════════════════════════════════════════════
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_MOSI     23
#define OLED_CLK      18
#define OLED_DC       21
#define OLED_CS       22
#define OLED_RESET    15

// ══════════════════════════════════════════════════════════════
// GLOBAL VARIABLES
// ══════════════════════════════════════════════════════════════
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC, OLED_RESET, OLED_CS);
WiFiClient espClient;
PubSubClient mqttClient(espClient);
bool wifiConnected = false;

// ══════════════════════════════════════════════════════════════
// WiFi & MQTT
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
  mqttClient.setBufferSize(512);
}

void reconnectMQTT() {
  if (mqttClient.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;
  
  Serial.print("[MQTT] Connecting...");
  
  if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.println(" connected!");
    mqttClient.subscribe(MQTT_OTA_TOPIC);
    Serial.print("[MQTT] Subscribed OTA topic: ");
    Serial.println(MQTT_OTA_TOPIC);
  } else {
    Serial.print(" failed, rc=");
    Serial.print(mqttClient.state());
    Serial.println(" retry in 5s");
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
    
    // Tampilkan di OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 10);
    display.println("OTA UPDATE");
    display.println("Downloading...");
    display.display();
    
    httpUpdate.rebootOnUpdate(true);
    
    Serial.println("Downloading firmware...");
    t_httpUpdate_return ret = httpUpdate.update(espClient, msg);
    
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
  }
}

// ══════════════════════════════════════════════════════════════
// SETUP
// ══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n================================");
  Serial.println("Device: OTA Testing (OLED Only)");
  Serial.print("Version: ");
  Serial.println(FIRMWARE_VERSION);
  Serial.print("Build: ");
  Serial.println(BUILD_DATE);
  Serial.println("================================\n");
  
  // Setup OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println("[OLED] FAILED! Check wiring.");
    for (;;) delay(1000);
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Tampilkan info versi di OLED
  display.setTextSize(2);
  display.setCursor(10, 10);
  display.println("OTA TEST");
  
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.print("Version: ");
  display.println(FIRMWARE_VERSION);
  
  display.setCursor(0, 40);
  display.print("Build: ");
  display.println(BUILD_DATE);
  
  display.setCursor(0, 50);
  display.println("Waiting for OTA...");
  display.display();
  
  delay(2000);
  
  // Setup WiFi & MQTT
  setupWiFi();
  setupMQTT();
  mqttClient.setCallback(mqttCallback);
  
  Serial.println("\n================================");
  Serial.println("  Ready! Waiting for OTA trigger");
  Serial.print("  OTA Topic: ");
  Serial.println(MQTT_OTA_TOPIC);
  Serial.println("================================\n");
}

// ══════════════════════════════════════════════════════════════
// LOOP
// ══════════════════════════════════════════════════════════════
void loop() {
  // WiFi reconnect
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
  
  // MQTT reconnect
  reconnectMQTT();
  mqttClient.loop();
  
  delay(50);
}
