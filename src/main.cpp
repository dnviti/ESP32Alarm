/*
ETHERNET CONNECTION 
ESP32-DevKitC-LAN8720
by Ziotester.it 
You do not need to edit this file to test ethernet connection with ESP32-DevKitC-LAN8720 by ZioTester.it with DHCP
More info on https://github.com/ZioTester/ESP32-DevKitC-LAN8720
*/

#include <Arduino.h>
#include <ETH.h>

/*///////////////////////////////////////////////
/////////////////////////////////////////////////
CONFIGURATION NEEDED BY LAN8720 CHIP
DO NOT EDIT!
/////////////////////////////////////////////////
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
//Nelle versioni precedenti il power pin è 12
#define ETH_POWER_PIN 17
#define ETH_ADDR 1
#define ETH_TYPE ETH_PHY_LAN8720
#define ETH_CLK_MODE ETH_CLOCK_GPIO0_IN
/////////////////////////////////////////////////

/////////////////////////////////////////////////


static bool eth_connected = false;
static const int MOTION_PIN = 32;
bool motionDetected = false;

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      Serial.println("ETH Started");
      ETH.setHostname("ziotester-eth");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected (SYSTEM_EVENT_ETH_CONNECTED)");
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

void setup() {
  Serial.begin(115200);
  pinMode(ETH_POWER_PIN, OUTPUT);
  delay(99);
  WiFi.onEvent(WiFiEvent);
  ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE,
            ETH_CLK_MODE);
  pinMode(MOTION_PIN, INPUT);

}

void loop() {
  int sensorVal = digitalRead(MOTION_PIN);
  if (sensorVal == HIGH) {
    if (!motionDetected) {
      motionDetected = true;
      Serial.println("Motion detected!");
      // ... do something in response
      // e.g. send data via Ethernet, log, etc.
    }
  } else {
    motionDetected = false;
  }
}