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
#include <esp_camera.h>

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
const char* AUTH_USER = "admin";
const char* AUTH_PASS = "password";

// Alarm system state
bool alarmArmed = false;

// Motion sensor pins and status
#define NUM_SENSORS 3
const int sensorPins[NUM_SENSORS] = {13, 14, 33};
bool sensorStatus[NUM_SENSORS];  // true means motion detected

// Web server and WebSocket instance
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// HTML webpage content stored in flash (PROGMEM) with vanilla JavaScript and WebSocket for real-time updates.
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; background: #f0f0f0; }
    h1 { color: #333; }
    .armed { color: red; font-weight: bold; }
    .disarmed { color: green; font-weight: bold; }
    .sensor-status { font-weight: bold; }
    .motion { color: red; }
    .nomotion { color: green; }
  </style>
</head><body>
  <h1>ESP32 Alarm System</h1>
  <p>System Status: <span id='armStatus' class='disarmed'>Disarmed</span></p>
  <button id='armBtn' onclick='toggleArm()'>Arm System</button>
  <h2>Motion Sensors</h2>
  <ul>
    <li>Room 1: <span id='sensor0' class='sensor-status nomotion'>No motion</span></li>
    <li>Room 2: <span id='sensor1' class='sensor-status nomotion'>No motion</span></li>
    <li>Room 3: <span id='sensor2' class='sensor-status nomotion'>No motion</span></li>
  </ul>
  <h2>Camera Feed</h2>
  <img id='camImg' src='/api/camera?id=1' alt='Camera feed' width='320'><br>
  <button onclick='refreshCamera()'>Refresh Camera</button>
  <script>
    var socket = new WebSocket('ws://' + window.location.host + '/ws');
    socket.onmessage = function(event) {
      var data = JSON.parse(event.data);
      if(data.type === 'init') {
        document.getElementById('armStatus').textContent = data.armed ? 'Armed' : 'Disarmed';
        document.getElementById('armStatus').className = data.armed ? 'armed' : 'disarmed';
        document.getElementById('armBtn').textContent = data.armed ? 'Disarm System' : 'Arm System';
        for(var i=0; i<data.sensors.length; i++){
          var statusElem = document.getElementById('sensor'+i);
          if(data.sensors[i]) {
            statusElem.textContent = 'Motion detected';
            statusElem.className = 'sensor-status motion';
          } else {
            statusElem.textContent = 'No motion';
            statusElem.className = 'sensor-status nomotion';
          }
        }
      } else if(data.type === 'alarm') {
        document.getElementById('armStatus').textContent = data.armed ? 'Armed' : 'Disarmed';
        document.getElementById('armStatus').className = data.armed ? 'armed' : 'disarmed';
        document.getElementById('armBtn').textContent = data.armed ? 'Disarm System' : 'Arm System';
      } else if(data.type === 'sensor') {
        var i = data.id;
        var statusElem = document.getElementById('sensor'+i);
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
    function refreshCamera() {
      var camImg = document.getElementById('camImg');
      camImg.src = '/api/camera?id=1&ts=' + new Date().getTime();
    }
  </script>
</body></html>
)rawliteral";

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

// Ethernet event callback function
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
      if (ETH.fullDuplex()) {
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

// WebSocket event handler for client connect/disconnect and messages.
void onWebSocketEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type,
                      void * arg, uint8_t * data, size_t len) {
  if(type == WS_EVT_CONNECT) {
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    // Send initialization data (current alarm state and sensor statuses)
    String initMsg = "{\"type\":\"init\",\"armed\":";
    initMsg += (alarmArmed ? "true" : "false");
    initMsg += ",\"sensors\":[";
    for(int i = 0; i < NUM_SENSORS; i++){
      initMsg += (sensorStatus[i] ? "true" : "false");
      if(i < NUM_SENSORS - 1) initMsg += ",";
    }
    initMsg += "]}";
    client->text(initMsg);
  } else if(type == WS_EVT_DISCONNECT) {
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
  } else if(type == WS_EVT_DATA) {
    // Handle incoming WebSocket commands (arm/disarm)
    data[len] = 0; // null-terminate
    String cmd = (char*)data;
    Serial.printf("Received WebSocket message: %s\n", cmd.c_str());
    if(cmd == "arm") {
      alarmArmed = true;
      Serial.println("System armed via WebSocket");
      ws.textAll("{\"type\":\"alarm\",\"armed\":true}");
    } else if(cmd == "disarm") {
      alarmArmed = false;
      Serial.println("System disarmed via WebSocket");
      ws.textAll("{\"type\":\"alarm\",\"armed\":false}");
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ETH_POWER_PIN, OUTPUT);
  delay(100); // Allow LAN8720 to power up
  WiFi.onEvent(WiFiEvent);
  ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);

  // Initialize motion sensor pins and statuses
  for(int i = 0; i < NUM_SENSORS; i++){
    pinMode(sensorPins[i], INPUT);
    sensorStatus[i] = digitalRead(sensorPins[i]);
  }

  // Camera configuration
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000; // 20 MHz
  config.pixel_format = PIXFORMAT_JPEG;
  // Optimize frame size for ESP-WROOM-32 (without PSRAM)
  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA; // 640x480
    config.jpeg_quality = 12;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA; // 320x240 for reduced TX buffer usage
    config.jpeg_quality = 15;
    config.fb_count = 1;
  }
  if(esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
  } else {
    Serial.println("Camera init succeeded");
  }

  // Setup WebSocket handler
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  // Serve web interface with authentication
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request){
    if(!request->authenticate(AUTH_USER, AUTH_PASS))
      return request->requestAuthentication();
    request->send_P(200, "text/html", index_html);
  });

  // REST API: Arm system
  server.on("/api/arm", HTTP_POST, [](AsyncWebServerRequest * request){
    if(!request->authenticate(AUTH_USER, AUTH_PASS))
      return request->requestAuthentication();
    alarmArmed = true;
    Serial.println("System armed via REST API");
    ws.textAll("{\"type\":\"alarm\",\"armed\":true}");
    request->send(200, "application/json", "{\"status\":\"armed\"}");
  });

  // REST API: Disarm system
  server.on("/api/disarm", HTTP_POST, [](AsyncWebServerRequest * request){
    if(!request->authenticate(AUTH_USER, AUTH_PASS))
      return request->requestAuthentication();
    alarmArmed = false;
    Serial.println("System disarmed via REST API");
    ws.textAll("{\"type\":\"alarm\",\"armed\":false}");
    request->send(200, "application/json", "{\"status\":\"disarmed\"}");
  });

  // REST API: Retrieve sensor statuses
  server.on("/api/sensors", HTTP_GET, [](AsyncWebServerRequest * request){
    if(!request->authenticate(AUTH_USER, AUTH_PASS))
      return request->requestAuthentication();
    String json = "{\"sensors\":[";
    for(int i = 0; i < NUM_SENSORS; i++){
      json += (sensorStatus[i] ? "true" : "false");
      if(i < NUM_SENSORS - 1)
        json += ",";
    }
    json += "],\"armed\":";
    json += (alarmArmed ? "true" : "false");
    json += "}";
    request->send(200, "application/json", json);
  });

  // REST API: Retrieve camera snapshot with chunked response to prevent TX buffer overflow.
  server.on("/api/camera", HTTP_GET, [](AsyncWebServerRequest * request){
    if(!request->authenticate(AUTH_USER, AUTH_PASS))
      return request->requestAuthentication();
    camera_fb_t * fb = esp_camera_fb_get();
    if(!fb) {
      request->send(500, "text/plain", "Camera capture failed");
      return;
    }
    // Send image in chunks
    request->sendChunked("image/jpeg", [fb](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
      size_t remaining = fb->len - index;
      size_t toSend = (remaining > maxLen) ? maxLen : remaining;
      if(toSend) {
        memcpy(buffer, fb->buf + index, toSend);
      }
      // Free the frame buffer when transmission is complete
      if(index + toSend >= fb->len) {
        esp_camera_fb_return(fb);
      }
      return toSend;
    });
  });

  // Start web server
  server.begin();
}

void loop() {
  // Monitor motion sensor states and notify via WebSocket on state change.
  for (int i = 0; i < NUM_SENSORS; i++) {
    bool state = digitalRead(sensorPins[i]);
    if (state != sensorStatus[i]) {
      sensorStatus[i] = state;
      Serial.printf("Sensor %d status changed to %s\n", i, state ? "MOTION" : "CLEAR");
      String msg = String("{\"type\":\"sensor\",\"id\":") + i +
                   ",\"status\":" + (state ? "true" : "false") + "}";
      ws.textAll(msg);
      // If alarm is armed and motion is detected, trigger further actions here.
      if (alarmArmed && state) {
        Serial.printf("*** ALARM! Motion detected on sensor %d ***\n", i);
      }
    }
  }
  // Clean up disconnected WebSocket clients.
  ws.cleanupClients();
  delay(100);
}
