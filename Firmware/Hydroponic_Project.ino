/*
  Hydroponic_PCB firmware  (ESP32-S3-WROOM-1 N8R2)
  - pH (analog), DHT22, SSD1306 OLED (I2C)
  - Telemetry -> ThingsBoard (MQTT)
  - Relay1 / Relay2 : auto timer + remote control (RPC)

  Libraries: PubSubClient, DHT sensor library, Adafruit GFX, Adafruit SSD1306
  Board    : ESP32S3 Dev Module
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- Credentials (DO NOT commit real values) ----------
const char* WIFI_SSID  = "YOUR_WIFI_SSID";
const char* WIFI_PASS  = "YOUR_WIFI_PASSWORD";
const char* TB_HOST    = "demo.thingsboard.io";
const char* TB_TOKEN   = "YOUR_THINGSBOARD_TOKEN";

// ---------- Pins (GPIO numbers, from the schematic) ----------
#define PH_PIN     4    // Po  -> IO4
#define RELAY1_PIN 35   // Relay1 -> IO35
#define RELAY2_PIN 36   // Relay2 -> IO36
#define DHT_PIN    40   // DHT22 -> IO40
#define SDA_PIN    8
#define SCL_PIN    9

// Relay is driven by an NPN 2N3904 => HIGH = relay ON
#define RELAY_ON   HIGH
#define RELAY_OFF  LOW

#define DHTTYPE        DHT22
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_ADDR      0x3C

// ---------- pH calibration (adjust with pH 4.0 / 7.0 buffer) ----------
float PH_V_NEUTRAL = 2.50;  // volts measured at pH 7.0
float PH_SLOPE     = 3.00;  // pH units per volt (sign/value from calibration)

// ---------- Timing ----------
const unsigned long PUMP_CYCLE_MS     = 15000;  // full cycle
const unsigned long PUMP_ON_MS        = 10000;  // ON time in each cycle
const unsigned long PUBLISH_INTERVAL  = 5000;   // telemetry every 5 s
const unsigned long SENSOR_INTERVAL   = 1000;   // read/display every 1 s

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHT dht(DHT_PIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

bool oledOk = false;
bool pumpManual = false;      // true = remote control overrides the timer
bool relay1State = false;
bool relay2State = false;

float ph = NAN, temp = NAN, humi = NAN;
unsigned long cycleStart = 0, lastPublish = 0, lastSensor = 0, lastMqttTry = 0;

// ---------- Relay helpers ----------
void setRelay1(bool on) {
  relay1State = on;
  digitalWrite(RELAY1_PIN, on ? RELAY_ON : RELAY_OFF);
}
void setRelay2(bool on) {
  relay2State = on;
  digitalWrite(RELAY2_PIN, on ? RELAY_ON : RELAY_OFF);
}

// ---------- Sensors ----------
float readPH() {
  // Average 16 samples; analogReadMilliVolts uses the chip's ADC calibration
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogReadMilliVolts(PH_PIN);
    delay(2);
  }
  float voltage = (sum / 16.0) / 1000.0;
  return 7.0 + (PH_V_NEUTRAL - voltage) * PH_SLOPE;
}

// ---------- OLED ----------
void updateOLED() {
  if (!oledOk) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("ESP32 Hydroponic ");
  display.println(client.connected() ? "*" : "-");
  display.setCursor(0, 16);
  display.printf("pH  : %.2f\n", ph);
  display.setCursor(0, 28);
  if (isnan(temp)) display.println("Temp: --");
  else             display.printf("Temp: %.1f C\n", temp);
  display.setCursor(0, 40);
  if (isnan(humi)) display.println("Humi: --");
  else             display.printf("Humi: %.1f %%\n", humi);
  display.setCursor(0, 54);
  display.printf("P1:%s P2:%s %s", relay1State ? "ON" : "OFF",
                 relay2State ? "ON" : "OFF", pumpManual ? "MAN" : "AUTO");
  display.display();
}

// ---------- MQTT / RPC ----------
// ThingsBoard RPC request:
//   topic   : v1/devices/me/rpc/request/<id>
//   payload : {"method":"setPump","params":true}
// Supported methods: setPump / setRelay1, setRelay2, setAuto
// Plain-text "ON" / "OFF" payloads are also accepted (control Relay1).
bool payloadWantsOn(const String& msg) {
  int p = msg.indexOf("params");
  String tail = (p >= 0) ? msg.substring(p) : msg;
  tail.toLowerCase();
  return tail.indexOf("true") >= 0 || tail.indexOf("\"on\"") >= 0 ||
         tail.indexOf("1") >= 0 || tail == "on";
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  String t(topic);
  Serial.printf("MQTT [%s] %s\n", topic, msg.c_str());

  bool handled = false;

  if (msg.indexOf("setAuto") >= 0) {
    pumpManual = false;
    handled = true;
  } else if (msg.indexOf("setRelay2") >= 0) {
    setRelay2(payloadWantsOn(msg));
    handled = true;
  } else if (msg.indexOf("setPump") >= 0 || msg.indexOf("setRelay1") >= 0) {
    pumpManual = true;
    setRelay1(payloadWantsOn(msg));
    handled = true;
  } else if (msg == "ON") {
    pumpManual = true;
    setRelay1(true);
    handled = true;
  } else if (msg == "OFF") {
    pumpManual = true;
    setRelay1(false);
    handled = true;
  }

  // Reply to RPC so the dashboard widget gets a response
  if (handled && t.startsWith("v1/devices/me/rpc/request/")) {
    String reqId = t.substring(t.lastIndexOf('/') + 1);
    String resp = "{\"relay1\":" + String(relay1State ? "true" : "false") +
                  ",\"relay2\":" + String(relay2State ? "true" : "false") +
                  ",\"manual\":" + String(pumpManual ? "true" : "false") + "}";
    client.publish(("v1/devices/me/rpc/response/" + reqId).c_str(), resp.c_str());
  }
}

void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) delay(250);
}

// Non-blocking reconnect: tries once every 5 s so the pump timer keeps running
void ensureMqtt() {
  if (client.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastMqttTry < 5000) return;
  lastMqttTry = millis();
  if (client.connect("ESP32-Hydroponic", TB_TOKEN, NULL)) {
    client.subscribe("v1/devices/me/rpc/request/+");
    Serial.println("MQTT connected");
  } else {
    Serial.printf("MQTT failed, rc=%d\n", client.state());
  }
}

void publishTelemetry() {
  if (!client.connected()) return;
  String payload = "{";
  payload += "\"ph\":" + String(ph, 2);
  if (!isnan(temp)) payload += ",\"temperature\":" + String(temp, 1);
  if (!isnan(humi)) payload += ",\"humidity\":" + String(humi, 1);
  payload += ",\"relay1\":" + String(relay1State ? "true" : "false");
  payload += ",\"relay2\":" + String(relay2State ? "true" : "false");
  payload += "}";
  client.publish("v1/devices/me/telemetry", payload.c_str());
}

// ---------- Arduino ----------
void setup() {
  Serial.begin(115200);

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  setRelay1(false);
  setRelay2(false);

  analogReadResolution(12);
  analogSetPinAttenuation(PH_PIN, ADC_11db);

  dht.begin();
  Wire.begin(SDA_PIN, SCL_PIN);
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (!oledOk) Serial.println("OLED not found");

  WiFi.mode(WIFI_STA);
  ensureWiFi();

  client.setServer(TB_HOST, 1883);
  client.setCallback(callback);

  cycleStart = millis();
}

void loop() {
  unsigned long now = millis();

  ensureWiFi();
  ensureMqtt();
  client.loop();

  // Read sensors + refresh display
  if (now - lastSensor >= SENSOR_INTERVAL) {
    lastSensor = now;
    ph = readPH();
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) temp = t;   // keep last good value if a read fails
    if (!isnan(h)) humi = h;
    updateOLED();
  }

  // Telemetry
  if (now - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = now;
    publishTelemetry();
  }

  // Automatic pump timer (ON for PUMP_ON_MS of every PUMP_CYCLE_MS)
  if (!pumpManual) {
    unsigned long diff = now - cycleStart;
    if (diff >= PUMP_CYCLE_MS) {
      cycleStart = now;
      diff = 0;
    }
    setRelay1(diff < PUMP_ON_MS);
  }
}