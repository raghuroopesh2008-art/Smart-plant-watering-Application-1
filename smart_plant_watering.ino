/*
 * Smart Plant Watering System — ESP32 firmware
 *
 * Reads soil moisture and air temperature/humidity, waters automatically
 * when the soil gets too dry, and serves a small JSON API that the
 * dashboard (dashboard/index.html) talks to over Wi-Fi.
 *
 * Endpoints:
 *   GET  /api/status          -> sensor readings and pump state
 *   POST /api/pump?state=on   -> manual pump control (on / off)
 *   POST /api/threshold?value=40 -> set auto-water threshold (%)
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

// ---------- Wi-Fi ----------
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ---------- Pins ----------
const int PIN_SOIL     = 34;   // Capacitive soil sensor (analog, input-only pin)
const int PIN_DHT      = 4;    // DHT11 data pin
const int PIN_PUMP     = 26;   // Relay IN pin that switches the pump
const bool RELAY_ACTIVE_LOW = true;  // Most relay boards are active-low

// ---------- Calibration ----------
// Read the sensor in air (dry) and in a cup of water (wet), then update these.
const int SOIL_DRY_RAW = 3100;   // ADC value in dry air
const int SOIL_WET_RAW = 1400;   // ADC value fully submerged

// ---------- Watering behaviour ----------
int  moistureThreshold     = 35;          // Water when soil % drops below this
const unsigned long PUMP_RUN_MS   = 5000; // How long each automatic watering lasts
const unsigned long COOLDOWN_MS   = 600000UL; // 10 min between automatic waterings
const unsigned long SAMPLE_MS     = 2000; // Sensor sampling interval

// ---------- State ----------
DHT dht(PIN_DHT, DHT11);
WebServer server(80);

int   soilPercent   = 0;
float airTemp       = 0;
float airHumidity   = 0;
bool  pumpOn        = false;
bool  manualMode    = false;     // true while the user is running the pump by hand
unsigned long pumpStartedAt = 0;
unsigned long lastAutoWater = 0;
unsigned long lastSample    = 0;
String lastEvent = "Booted";

// ---------- Helpers ----------
void setPump(bool on) {
  pumpOn = on;
  digitalWrite(PIN_PUMP, (on ^ RELAY_ACTIVE_LOW) ? HIGH : LOW);
}

int rawToPercent(int raw) {
  int p = map(raw, SOIL_DRY_RAW, SOIL_WET_RAW, 0, 100);
  return constrain(p, 0, 100);
}

void readSensors() {
  // Average several samples to smooth out noise
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(PIN_SOIL);
    delay(5);
  }
  soilPercent = rawToPercent(sum / 10);

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t)) {
    airHumidity = h;
    airTemp     = t;
  }
}

void sendJson(int code, const String& body) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(code, "application/json", body);
}

// ---------- HTTP handlers ----------
void handleStatus() {
  String json = "{";
  json += "\"soil\":" + String(soilPercent) + ",";
  json += "\"temperature\":" + String(airTemp, 1) + ",";
  json += "\"humidity\":" + String(airHumidity, 0) + ",";
  json += "\"pump\":" + String(pumpOn ? "true" : "false") + ",";
  json += "\"threshold\":" + String(moistureThreshold) + ",";
  json += "\"manual\":" + String(manualMode ? "true" : "false") + ",";
  json += "\"lastEvent\":\"" + lastEvent + "\",";
  json += "\"uptimeMs\":" + String(millis());
  json += "}";
  sendJson(200, json);
}

void handlePump() {
  if (!server.hasArg("state")) {
    sendJson(400, "{\"error\":\"missing state\"}");
    return;
  }
  String s = server.arg("state");
  if (s == "on") {
    manualMode = true;
    setPump(true);
    lastEvent = "Manual watering started";
  } else if (s == "off") {
    manualMode = false;
    setPump(false);
    lastEvent = "Manual watering stopped";
  } else {
    sendJson(400, "{\"error\":\"state must be on or off\"}");
    return;
  }
  sendJson(200, "{\"pump\":" + String(pumpOn ? "true" : "false") + "}");
}

void handleThreshold() {
  if (!server.hasArg("value")) {
    sendJson(400, "{\"error\":\"missing value\"}");
    return;
  }
  moistureThreshold = constrain(server.arg("value").toInt(), 0, 100);
  lastEvent = "Threshold set to " + String(moistureThreshold) + "%";
  sendJson(200, "{\"threshold\":" + String(moistureThreshold) + "}");
}

// ---------- Automatic watering logic ----------
void autoWaterLogic() {
  unsigned long now = millis();

  // Stop the pump when its run time is up (automatic runs only)
  if (pumpOn && !manualMode && now - pumpStartedAt >= PUMP_RUN_MS) {
    setPump(false);
    lastEvent = "Watering finished";
  }

  // Skip automatic decisions while the user is in manual mode
  if (manualMode) return;

  bool dry     = soilPercent < moistureThreshold;
  bool cooled  = (lastAutoWater == 0) || (now - lastAutoWater >= COOLDOWN_MS);

  if (dry && cooled && !pumpOn) {
    setPump(true);
    pumpStartedAt = now;
    lastAutoWater = now;
    lastEvent = "Soil dry (" + String(soilPercent) + "%), watering";
  }
}

// ---------- Setup & loop ----------
void setup() {
  Serial.begin(115200);

  pinMode(PIN_PUMP, OUTPUT);
  setPump(false);            // Make sure the pump starts off

  dht.begin();
  analogReadResolution(12);  // 0–4095 range on ESP32

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected. Dashboard API at http://");
  Serial.println(WiFi.localIP());

  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/pump", HTTP_POST, handlePump);
  server.on("/api/threshold", HTTP_POST, handleThreshold);
  server.onNotFound([]() { sendJson(404, "{\"error\":\"not found\"}"); });
  server.begin();

  readSensors();
}

void loop() {
  server.handleClient();

  if (millis() - lastSample >= SAMPLE_MS) {
    lastSample = millis();
    readSensors();
    autoWaterLogic();
  }
}
