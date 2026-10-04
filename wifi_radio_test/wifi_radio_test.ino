#include <WiFi.h>

// Replace with your desired network credentials
const char* ssid = "ESP32-C3-SuperMini-AP";
const char* password = "yourpassword123"; // Leave empty "" for an open network
static const wifi_power_t TEST_TX_POWER = WIFI_POWER_8_5dBm;
static uint32_t lastReport = 0;
static bool apStarted = false;

void setup() {
  Serial.begin(115200);
  const uint32_t serialWaitStarted = millis();
  while (!Serial && millis() - serialWaitStarted < 3000) delay(10);

  Serial.println("Setting up Access Point (SoftAP)...");

  const bool modeStarted = WiFi.mode(WIFI_AP);
  const bool powerSet = WiFi.setTxPower(TEST_TX_POWER);
  apStarted = WiFi.softAP(ssid, password, 1, false, 4);

  Serial.printf("RADIO TEST: mode=%s, tx-power=%s (8.5 dBm), AP=%s\n",
                modeStarted ? "OK" : "FAILED",
                powerSet ? "set" : "FAILED",
                apStarted ? "started" : "FAILED");
  Serial.printf("RADIO TEST: SSID=%s, IP=%s\n", ssid, WiFi.softAPIP().toString().c_str());
}

void loop() {
  const uint32_t now = millis();
  if (now - lastReport >= 2000) {
    lastReport = now;
    Serial.printf("RADIO TEST: alive=%lu ms, AP=%s, clients=%u, heap=%u\n",
                  static_cast<unsigned long>(now),
                  apStarted ? "ON" : "OFF",
                  WiFi.softAPgetStationNum(),
                  static_cast<unsigned int>(ESP.getFreeHeap()));
  }
}
