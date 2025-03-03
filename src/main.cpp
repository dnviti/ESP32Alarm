/*
ETHERNET CONNECTION 
ESP32-DevKitC-LAN8720
by Ziotester.it 
You do not need to edit this file to test ethernet connection with 
ESP32-DevKitC-LAN8720 by ZioTester.it with DHCP
More info on https://github.com/ZioTester/ESP32-DevKitC-LAN8720
*/

#include <Arduino.h>
#include <ETH.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

/*///////////////////////////////////////////////
CONFIGURATION FOR LAN8720 CHIP
DO NOT EDIT!
///////////////////////////////////////////////*/
#define ETH_CLOCK_IN_PIN 0
#define ETH_MDIO_PIN 18
#define ETH_TXD0_PIN 19
#define ETH_TXEN_PIN 21
#define ETH_TXD1_PIN 22
#define ETH_MDC_PIN 23
#define ETH_RXD0_PIN 25
#define ETH_RXD1_PIN 26
#define ETH_MODE2_PIN 27
#define ETH_POWER_PIN 17
#define ETH_ADDR 1
#define ETH_TYPE ETH_PHY_LAN8720
#define ETH_CLK_MODE ETH_CLOCK_GPIO0_IN
/////////////////////////////////////////////////

// REST API authentication credentials
const char* AUTH_USER = "admin";
const char* AUTH_PASS = "password";

// Alarm system state and sensor configuration
bool alarmArmed = false;
#define NUM_SENSORS 8
const int sensorPins[NUM_SENSORS] = {32, 33, 36, 39, 34, 35, 14, 13};
bool sensorStatus[NUM_SENSORS];  // true = motion detected
int sirenPin = 15;

AsyncWebServer server(80);

// Camera module pin definitions (for AI-Thinker ESP32-CAM module)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

static bool eth_connected = false;

// Ethernet event callback
void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      Serial.println("ETH Started");
      ETH.setHostname("esp32-eth-alarm");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected (LAN cable linked)");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      Serial.print("ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
      Serial.print(ETH.localIP());
      if (ETH.fullDuplex()) { Serial.print(", FULL_DUPLEX"); }
      Serial.print(", ");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      eth_connected = true;
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case SYSTEM_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ETH_POWER_PIN, OUTPUT);
  delay(100); // Allow LAN8720 to power up
  WiFi.onEvent(WiFiEvent);
  ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);

  // Initialize motion sensor pins and read initial state
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(sensorPins[i], INPUT);
    sensorStatus[i] = digitalRead(sensorPins[i]);
  }

  // Initialize siren pin
  pinMode(sirenPin, OUTPUT);

  // REST API: Arm system
  server.on("/api/arm", HTTP_POST, [](AsyncWebServerRequest * request) {
    if (!request->authenticate(AUTH_USER, AUTH_PASS))
      return request->requestAuthentication();
    alarmArmed = true;
    Serial.println("System armed via REST API");
    request->send(200, "application/json", "{\"status\":\"armed\"}");
  });

  // REST API: Disarm system
  server.on("/api/disarm", HTTP_POST, [](AsyncWebServerRequest * request) {
    if (!request->authenticate(AUTH_USER, AUTH_PASS))
      return request->requestAuthentication();
    alarmArmed = false;
    Serial.println("System disarmed via REST API");
    request->send(200, "application/json", "{\"status\":\"disarmed\"}");
  });

  // REST API: Retrieve sensor statuses
  server.on("/api/sensors", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (!request->authenticate(AUTH_USER, AUTH_PASS))
      return request->requestAuthentication();
    String json = "{\"sensors\":[";
    for (int i = 0; i < NUM_SENSORS; i++) {
      json += (sensorStatus[i] ? "true" : "false");
      if (i < NUM_SENSORS - 1) json += ",";
    }
    json += "],\"armed\":";
    json += (alarmArmed ? "true" : "false");
    json += "}";
    request->send(200, "application/json", json);
  });

  // Start REST API server
  server.begin();
}

void loop() {
  // Continuously monitor sensors and update sensorStatus variable.
  for (int i = 0; i < NUM_SENSORS; i++) {
    bool state = digitalRead(sensorPins[i]);
    if (state != sensorStatus[i]) {
      sensorStatus[i] = state;
      Serial.printf("Sensor %d status changed to %s\n", i, state ? "MOTION" : "CLEAR");
      if (alarmArmed && state) {
        Serial.printf("*** ALARM! Motion detected on sensor %d ***\n", i);
        digitalWrite(sirenPin, HIGH);
        
        // Sound the siren for up to 5 minutes, but allow early exit if alarmArmed becomes false.
        unsigned long startTime = millis();
        while (millis() - startTime < 300000) { // 300000 ms = 5 minutes
          Serial.printf("*** ALARM! SCREAMING! ***\n");
          if (!alarmArmed) {  // If alarmArmed is false, break out early.
            break;
          }
          delay(100); // Small delay to avoid hogging the CPU.
        }
        
        digitalWrite(sirenPin, LOW); // Turn off the siren.
      }
    }
  }
  delay(100);
}

