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
/////////////////////////////////////////////////
CONFIGURATION NEEDED BY LAN8720 CHIP
DO NOT EDIT!
///////////////////////////////////////////////
//////////////////////////////////////////////*/

#define ETH_CLOCK_IN_PIN 0
#define ETH_MDIO_PIN 18
#define ETH_TXD0_PIN 19
#define ETH_TXEN_PIN 21
#define ETH_TXD1_PIN 22
#define ETH_MDC_PIN 23
#define ETH_RXD0_PIN 25
#define ETH_RXD1_PIN 26
#define ETH_MODE2_PIN 27
// In earlier versions, the power pin was 12
#define ETH_POWER_PIN 17
#define ETH_ADDR 1
#define ETH_TYPE ETH_PHY_LAN8720
#define ETH_CLK_MODE ETH_CLOCK_GPIO0_IN
/////////////////////////////////////////////////

/////////////////////////////////////////////////

// Authentication credentials for REST API and web interface
// NEED TO CHANGE!
const char *AUTH_USER = "admin";
const char *AUTH_PASS = "password";

// Alarm system state
bool alarmArmed = false;

// Motion sensor pins and status
#define NUM_SENSORS 8
const int sensorPins[NUM_SENSORS] = {32, 33, 36, 39, 34, 35, 14, 13};
bool sensorStatus[NUM_SENSORS];
// (Assume sensors are wired such that HIGH = motion detected, LOW = no motion)
int sirenPin = 15;

// Web server and WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// HTML webpage content (served by ESP32) with embedded JavaScript for UI
const char index_html[] PROGMEM = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 Alarm System</title>
    <link href="https://fonts.googleapis.com/css?family=Roboto:400,700&display=swap" rel="stylesheet">
    <style>
      body {
        margin: 0;
        font-family: 'Roboto', sans-serif;
        background: linear-gradient(135deg, #2d3436, #636e72);
        color: #fff;
      }
      .container {
        max-width: 800px;
        margin: 40px auto;
        padding: 20px 30px;
        background: rgba(0, 0, 0, 0.6);
        border-radius: 10px;
        box-shadow: 0 4px 10px rgba(0, 0, 0, 0.3);
      }
      h1 {
        font-size: 2.5em;
        margin-bottom: 10px;
        text-shadow: 2px 2px 4px rgba(0,0,0,0.7);
      }
      .status-card {
        display: flex;
        justify-content: space-between;
        align-items: center;
        padding: 15px;
        background: rgba(255,255,255,0.1);
        border-radius: 8px;
        margin-bottom: 20px;
      }
      .status-card span {
        font-size: 1.2em;
        font-weight: bold;
      }
      .armed {
        color: #e74c3c;
      }
      .disarmed {
        color: #2ecc71;
      }
      button {
        background: #3498db;
        border: none;
        padding: 10px 20px;
        font-size: 1em;
        color: #fff;
        border-radius: 5px;
        cursor: pointer;
        transition: background 0.3s ease;
        margin-bottom: 20px;
      }
      button:hover {
        background: #2980b9;
      }
      h2 {
        font-size: 2em;
        border-bottom: 2px solid rgba(255, 255, 255, 0.3);
        padding-bottom: 10px;
        margin-top: 30px;
      }
      ul {
        list-style-type: none;
        padding: 0;
      }
      li {
        display: flex;
        justify-content: space-between;
        align-items: center;
        padding: 10px;
        margin: 10px 0;
        background: rgba(255, 255, 255, 0.1);
        border-radius: 5px;
        transition: background 0.3s ease;
      }
      li:hover {
        background: rgba(255, 255, 255, 0.2);
      }
      .sensor-status {
        font-weight: bold;
      }
      .motion {
        color: #e74c3c;
      }
      .nomotion {
        color: #2ecc71;
      }
      footer {
        text-align: center;
        margin-top: 40px;
        font-size: 0.9em;
        color: rgba(255, 255, 255, 0.7);
      }
    </style>
  </head>
  <body>
    <div class="container">
      <h1>ESP32 Alarm System</h1>
      <div class="status-card">
        <p>System Status:</p>
        <span id="armStatus" class="disarmed">Disarmed</span>
      </div>
      <button id="armBtn" onclick="toggleArm()">Arm System</button>
      <h2>Motion Sensors</h2>
      <ul id="sensorsList"></ul>
    </div>
    <footer>
      &copy; 2025 ESP32 Alarm System
    </footer>
    <script>
      var socket = new WebSocket('ws://' + window.location.host + '/ws');
      socket.onmessage = function(event) {
        var data = JSON.parse(event.data);
        if(data.type === 'init') {
          // Update armed/disarmed status
          document.getElementById('armStatus').textContent = data.armed ? 'Armed' : 'Disarmed';
          document.getElementById('armStatus').className = data.armed ? 'armed' : 'disarmed';
          document.getElementById('armBtn').textContent = data.armed ? 'Disarm System' : 'Arm System';
          
          // Dynamically generate sensor list items based on the sensors array length
          var sensorsList = document.getElementById('sensorsList');
          sensorsList.innerHTML = ''; // clear any existing items
          for(var i = 0; i < data.sensors.length; i++){
            var li = document.createElement('li');
            li.innerHTML = 'Room ' + (i+1) + ': <span id="sensor' + i + '" class="sensor-status nomotion">No motion</span>';
            sensorsList.appendChild(li);
          }
          
          // Set initial sensor statuses
          for(var i = 0; i < data.sensors.length; i++){
            var statusElem = document.getElementById('sensor' + i);
            if(data.sensors[i]) {
              statusElem.textContent = 'Motion detected';
              statusElem.className = 'sensor-status motion';
            } else {
              statusElem.textContent = 'No motion';
              statusElem.className = 'sensor-status nomotion';
            }
          }
        } else if(data.type === 'alarm') {
          // Update armed/disarmed status
          document.getElementById('armStatus').textContent = data.armed ? 'Armed' : 'Disarmed';
          document.getElementById('armStatus').className = data.armed ? 'armed' : 'disarmed';
          document.getElementById('armBtn').textContent = data.armed ? 'Disarm System' : 'Arm System';
        } else if(data.type === 'sensor') {
          // Update a single sensor's status
          var i = data.id;
          var statusElem = document.getElementById('sensor' + i);
          if(data.status) {
            statusElem.textContent = 'Motion detected';
            statusElem.className = 'sensor-status motion';
          } else {
            statusElem.textContent = 'No motion';
            statusElem.className = 'sensor-status nomotion';
          }
        }
      };
      socket.onopen = function(event) { console.log('WebSocket Connected'); };
      socket.onclose = function(event) { console.log('WebSocket Closed'); };
  
      function toggleArm() {
        if(document.getElementById('armStatus').textContent === 'Disarmed') {
          socket.send('arm');
        } else {
          socket.send('disarm');
        }
      }
    </script>
  </body>
  </html>
  )rawliteral";

// Camera module pin definitions (for AI-Thinker ESP32-CAM module)
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

static bool eth_connected = false;

// Ethernet event callback
void WiFiEvent(WiFiEvent_t event)
{
  switch (event)
  {
  case SYSTEM_EVENT_ETH_START:
    Serial.println("ETH Started");
    // Set Hostname for ESP32
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
    if (ETH.fullDuplex())
    {
      Serial.print(", FULL_DUPLEX");
    }
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

// WebSocket event handling function
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                      void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    // Send current alarm state and sensor status to the newly connected client
    String initMsg = "{\"type\":\"init\",\"armed\":";
    initMsg += (alarmArmed ? "true" : "false");
    initMsg += ",\"sensors\":[";
    for (int i = 0; i < NUM_SENSORS; i++)
    {
      initMsg += (sensorStatus[i] ? "true" : "false");
      if (i < NUM_SENSORS - 1)
        initMsg += ",";
    }
    initMsg += "]}";
    client->text(initMsg);
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
  }
  else if (type == WS_EVT_DATA)
  {
    // Handle incoming WebSocket data (commands from client)
    data[len] = 0; // Null-terminate the received data (ensure the buffer is large enough!)
    String cmd = (char *)data;
    Serial.printf("Received WebSocket message: %s\n", cmd.c_str());
    if (cmd == "arm")
    {
      alarmArmed = true;
      Serial.println("System armed via WebSocket");
      ws.textAll("{\"type\":\"alarm\",\"armed\":true}");
    }
    else if (cmd == "disarm")
    {
      alarmArmed = false;
      Serial.println("System disarmed via WebSocket");
      ws.textAll("{\"type\":\"alarm\",\"armed\":false}");
    }
    // (Handle other commands if needed)
  }
}

// Helper function to broadcast sensor updates
void notifySensorChange(int sensorId, bool status)
{
  String sensorMsg = "{\"type\":\"sensor\",\"id\":" + String(sensorId) + ",\"status\":" + (status ? "true" : "false") + "}";
  ws.textAll(sensorMsg);
  Serial.println("Broadcast sensor update: " + sensorMsg);
}

void notifyAlarmChange(bool armed)
{
  String alarmMsg = "{\"type\":\"alarm\",\"armed\":" + String(armed ? "true" : "false") + "}";
  ws.textAll(alarmMsg);
  Serial.println("Broadcast alarm update: " + alarmMsg);
}

void setup()
{
  Serial.begin(115200);
  pinMode(ETH_POWER_PIN, OUTPUT);
  // Give LAN8720 some time to power up
  delay(100);
  WiFi.onEvent(WiFiEvent); // Attach event handler for Ethernet
  ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);

  IPAddress ipAddress;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns;
  ipAddress.fromString("192.168.128.40");
  gateway.fromString("192.168.128.1");
  subnet.fromString("255.255.255.0");
  dns.fromString("192.168.128.1");
  ETH.config(ipAddress, gateway, subnet, dns);

  // Configure motion sensor pins as inputs
  for (int i = 0; i < NUM_SENSORS; i++)
  {
    pinMode(sensorPins[i], INPUT);
    sensorStatus[i] = digitalRead(sensorPins[i]); // read initial status
  }

  pinMode(sirenPin, OUTPUT);

  // Configure WebSocket handler and start server
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  // Serve the main webpage
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
  if (!request->authenticate(AUTH_USER, AUTH_PASS)) {
    return request->requestAuthentication();
  }
  request->send_P(200, "text/html", index_html); });

  // REST API: arm system
  server.on("/api/arm", HTTP_POST, [](AsyncWebServerRequest *request)
            {
  if (!request->authenticate(AUTH_USER, AUTH_PASS)) {
    return request->requestAuthentication();
  }
  alarmArmed = true;
  Serial.println("System armed via REST API");
  notifyAlarmChange(true);
  request->send(200, "application/json", "{\"status\":\"armed\"}"); });

  // REST API: disarm system
  server.on("/api/disarm", HTTP_POST, [](AsyncWebServerRequest *request)
            {
  if (!request->authenticate(AUTH_USER, AUTH_PASS)) {
    return request->requestAuthentication();
  }
  alarmArmed = false;
  Serial.println("System disarmed via REST API");
  notifyAlarmChange(false);
  request->send(200, "application/json", "{\"status\":\"disarmed\"}"); });

  // REST API: get sensor status
  server.on("/api/sensors", HTTP_GET, [](AsyncWebServerRequest *request)
            {
  if (!request->authenticate(AUTH_USER, AUTH_PASS)) {
    return request->requestAuthentication();
  }
  String json = "{\"sensors\":[";
  for (int i = 0; i < NUM_SENSORS; i++) {
    json += (sensorStatus[i] ? "true" : "false");
    if (i < NUM_SENSORS - 1) json += ",";
  }
  json += "],\"armed\":";
  json += (alarmArmed ? "true" : "false");
  json += "}";
  request->send(200, "application/json", json); });

  // Start web server
  server.begin();
}

void loop()
{
  // Continuously monitor sensors and update sensorStatus variable.
  for (int i = 0; i < NUM_SENSORS; i++)
  {
    bool state = digitalRead(sensorPins[i]);
    if (state != sensorStatus[i])
    {
      sensorStatus[i] = state;
      Serial.printf("Sensor %d status changed to %s\n", i, state ? "MOTION" : "CLEAR");

      // Notify WebSocket clients of the sensor status change.
      notifySensorChange(i, state);

      if (alarmArmed && state)
      {
        Serial.printf("*** ALARM! Motion detected on sensor %d ***\n", i);
        digitalWrite(sirenPin, HIGH);

        // Sound the siren for up to 5 minutes, but allow early exit if alarmArmed becomes false.
        unsigned long startTime = millis();
        while (millis() - startTime < 300000)
        { // 300000 ms = 5 minutes
          if (!alarmArmed)
          { // If alarmArmed is false, break out early.
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
