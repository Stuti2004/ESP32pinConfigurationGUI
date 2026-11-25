//main.cpp
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <FS.h>
#include <SPIFFS.h>
#include <math.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <ArduinoJson.h>  
#include "config.h"

//// ----------------- Debug / Serial.printlnging -----------------
//#define DEBUG true
//#define Serial.println_FILE "/Serial.println.txt"
//#define Serial.println(msg) if (DEBUG) { Serial.println(msg); }

// ----------------- Hardware -----------------
Adafruit_ADS1115 ads;

// ADS1115 / NTC constants
bool adsInitialized = false;
#define BETA 3942.0
#define R25 20000.0
#define T25 298.15
const float VCC = 3.3;
const float R_FIXED = 19400.0;
const int SDA_PIN = 22;
const int SCL_PIN = 21;

// ----------------- Networking / Retry -----------------
int httpFailureCount = 0;
const int MAX_HTTP_FAILURES = 5;      // after this, schedule reconnects (no restart)
bool wifiNeedsReconnect = false;
unsigned long lastWifiCheckMs = 0;
const unsigned long WIFI_CHECK_INTERVAL_MS = 5000;  // check Wi‑Fi every 5 s
unsigned long nextReconnectAttemptMs = 0;
unsigned long reconnectBackoffMs = 3000;            // start with 3 s
const unsigned long RECONNECT_BACKOFF_MAX_MS = 30000;

// ----------------- App timing -----------------
unsigned long previousMillis = 0;
const unsigned long interval = 800;   // sensing/actuation period (ms)

// ----------------- Last-known-good (LKG) commands -----------------
int   lastRelayStatus = LOW;      // digital level: LOW=OFF, HIGH=ON
float lastFanSpeedCmd = 0.0f;     // percent 0–100 for VFD
float lastValveCmd    = 0.0f;     // percent 0–100
bool  haveRelayCmd = false;
bool  haveFanCmd   = false;
bool  haveValveCmd = false;

// ----------------- LEDs -----------------
void setRGB(bool red, bool green, bool blue) {
  digitalWrite(Red_RGB,   red   ? HIGH : LOW);
  digitalWrite(Green_RGB, green ? HIGH : LOW);
  digitalWrite(Blue_RGB,  blue  ? HIGH : LOW);
}

void blinkRed(unsigned long currentMillis) {
  static unsigned long previousBlinkMillis = 0;
  static bool ledState = false;
  if (currentMillis - previousBlinkMillis >= 1000) {
    previousBlinkMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(Red_RGB, ledState ? HIGH : LOW);
    digitalWrite(Green_RGB, LOW);
    digitalWrite(Blue_RGB, LOW);
  }
}


// ----------------- Reset helper -----------------
void performFullReset() {
  WiFiManager wm;
  wm.resetSettings();     // erase saved SSID/password
  delay(1000);
  ESP.restart();
}
// ----------------- Wi‑Fi setup & maintenance -----------------
void setupWiFiManager() {
  WiFiManager wm;
  wm.setConfigPortalTimeout(60);
  setRGB(true, false, false); // red while starting portal
  Serial.println("Starting WiFiManager portal (Building Management System)...");

  if (!wm.autoConnect("Building Management System")) {
    Serial.println("WiFiManager: failed to connect (timeout). Will continue offline.");
    setRGB(true, false, false); // stay red if offline
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to Wi‑Fi.");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    setRGB(false, true, false); // green
    delay(500);
  } else {
    Serial.println("Wi‑Fi not connected after portal.");
    setRGB(true, false, false); // red
  }
}

void wifiMaintain() {
  unsigned long now = millis();

  // Periodic health check
  if (now - lastWifiCheckMs >= WIFI_CHECK_INTERVAL_MS) {
    lastWifiCheckMs = now;
    if (WiFi.status() != WL_CONNECTED) {
      wifiNeedsReconnect = true;
    }
  }

  // Reconnect attempts with back-off (non-blocking)
  if (wifiNeedsReconnect && now >= nextReconnectAttemptMs) {
    Serial.println("Wi‑Fi reconnect attempt...");
    WiFi.reconnect();
    nextReconnectAttemptMs = now + reconnectBackoffMs;
    reconnectBackoffMs = min(reconnectBackoffMs * 2, RECONNECT_BACKOFF_MAX_MS);
  }

  // If connected, clear flags and show green LED
  if (WiFi.status() == WL_CONNECTED) {
    wifiNeedsReconnect = false;
    reconnectBackoffMs = 3000; // reset backoff
    setRGB(false, true, false); // green
  } else {
    // blink red when offline
    blinkRed(now);
  }
}

//// ----------------- Serial.printlnging -----------------
//void Serial.printlnMessage(String msg) {
//  String timeStamped = "[" + String(millis()) + "ms] " + msg;
//  Serial.println(timeStamped);
//
//  File file = SPIFFS.open(Serial.println_FILE, FILE_READ);
//  String content = "";
//  int lineCount = 0;
//  if (file) {
//    while (file.available()) {
//      String line = file.readStringUntil('\n');
//      content += line + "\n";
//      lineCount++;
//    }
//    file.close();
//  }
//  const int maxLines = 100;
//  if (lineCount > maxLines) {
//    int skipLines = lineCount - maxLines;
//    int idx = 0;
//    for (int i = 0; i < skipLines; i++) {
//      idx = content.indexOf('\n', idx) + 1;
//      if (idx <= 0) break;
//    }
//    if (idx > 0) content = content.substring(idx);
//  }
//  content += timeStamped + "\n";
//  file = SPIFFS.open(Serial.println_FILE, FILE_WRITE);
//  if (file) {
//    file.print(content);
//    file.close();
//  }
//}

// ----------------- Firebase helpers -----------------
static void configHttpClient(HTTPClient& http) {
  // Short timeouts so we don't block control loop
  http.setConnectTimeout(2000); // ms
  http.setTimeout(2000);        // ms for overall transfer
}

void firebasePATCH(String path, const String& jsonData) {
  if (WiFi.status() != WL_CONNECTED) {
    wifiNeedsReconnect = true;
    return;
  }

  HTTPClient http;
  String url = String(FIREBASE_HOST) + path + ".json?auth=" + FIREBASE_AUTH;
  configHttpClient(http);
  if (!http.begin(url)) {
    Serial.println("HTTP begin() failed for PATCH " + path);
    httpFailureCount++;
    if (httpFailureCount >= MAX_HTTP_FAILURES) wifiNeedsReconnect = true;
    return;
  }

  http.addHeader("Content-Type", "application/json");
  int code = http.PATCH(jsonData);
  if (code == 200) {
    Serial.println("PATCH " + path + " -> " + jsonData);
    httpFailureCount = 0;
  } else {
    Serial.println("PATCH failed (" + String(code) + "): " + http.errorToString(code));
    httpFailureCount++;
    if (httpFailureCount >= MAX_HTTP_FAILURES) {
      Serial.println("Too many HTTP failures. Will try Wi‑Fi reconnect (no restart).");
      wifiNeedsReconnect = true;
    }
  }
  http.end();
}

// NEW: single GET to fetch ALL command values at once
void receiveAllCommandsFromFirebase() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiNeedsReconnect = true;
    return; // keep applying cached outputs
  }

  HTTPClient http;
  String url = String(FIREBASE_HOST) + "/ahu/ahu5/data.json?auth=" + FIREBASE_AUTH;
  configHttpClient(http);
  if (!http.begin(url)) {
    Serial.println("HTTP begin() failed for GET /ahu/ahu5/data");
    httpFailureCount++;
    if (httpFailureCount >= MAX_HTTP_FAILURES) wifiNeedsReconnect = true;
    return;
  }

  int code = http.GET();
  if (code == 200) {
    String payload = http.getString();
    http.end();

    // Parse JSON
    StaticJsonDocument<4096> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.println(String("JSON parse error: ") + err.c_str());
      return;
    }
    httpFailureCount = 0;

    // Relay command
    if (!doc["status"].isNull()) {
      int status = doc["status"].as<int>();
      lastRelayStatus = status ? HIGH : LOW;
      haveRelayCmd = true;
      Serial.println("Commands: relay=" + String(status));
    } else {
      Serial.println("Commands: status not present, keeping relay=" + String(lastRelayStatus == HIGH ? 1 : 0));
    }

    // Fan speed command
    if (!doc["vfd"]["fanspeed"].isNull()) {
      float sp = doc["vfd"]["fanspeed"].as<float>();
      lastFanSpeedCmd = sp < 0 ? 0 : (sp > 100 ? 100 : sp);
      haveFanCmd = true;
      Serial.println("Commands: fan=" + String(lastFanSpeedCmd, 1) + "%");
    } else {
      Serial.println("Commands: vfd/fanspeed missing, keeping fan=" + String(lastFanSpeedCmd, 1) + "%");
    }

    // Valve command
    if (!doc["valve"]["valvecontrol"].isNull()) {
      float vp = doc["valve"]["valvecontrol"].as<float>();
      lastValveCmd = vp < 0 ? 0 : (vp > 100 ? 100 : vp);
      haveValveCmd = true;
      Serial.println("Commands: valve=" + String(lastValveCmd, 1) + "%");
    } else {
      Serial.println("Commands: valve/valvecontrol missing, keeping valve=" + String(lastValveCmd, 1) + "%");
    }

    // Optional remote reset flag (handled here to avoid second GET)
    if (!doc["feedback"]["reset"].isNull()) {
      int reset_switch = doc["feedback"]["reset"].as<int>();
      if (reset_switch == 1) {
        performFullReset();
      }
    }

  } else {
    Serial.println("GET /ahu/ahu5/data failed (" + String(code) + "): " + http.errorToString(code));
    http.end();
    httpFailureCount++;
    if (httpFailureCount >= MAX_HTTP_FAILURES) {
      wifiNeedsReconnect = true;
    }
    // keep applying cached outputs
  }
}

// ----------------- Control helpers -----------------
static float clamp01pct(float x) {
  if (x < 0.0f)   return 0.0f;
  if (x > 100.0f) return 100.0f;
  return x;
}

void setPumpSpeedPercent(float percent, int dacPin) {
  percent = clamp01pct(percent);
  // 0–10 V scaling via external driver; ESP32 built-in DAC is 0–3.3 V
  float vOutDesired = percent / 100.0f * 10.0f; // desired 0–10 V
  float vEsp32 = vOutDesired / 3.03f;           // your scaling factor
  int dacValue = (int)(vEsp32 / 3.3f * 255.0f);
  dacValue = constrain(dacValue, 0, 255);
  dacWrite(dacPin, dacValue);
}

// ----------------- Apply outputs (no GETs here) -----------------
void controlAHURelays() {
  // Just apply cached state; receiveAllCommandsFromFirebase() updates it
  digitalWrite(AHU1_RELAY_PIN, lastRelayStatus);
  Serial.println("Apply relay: " + String(lastRelayStatus == HIGH ? 1 : 0));
}

void controlFanSpeed() {
  // Just apply cached setpoints; receiveAllCommandsFromFirebase() updates them
  setPumpSpeedPercent(lastFanSpeedCmd, DAC1_PIN);
  setPumpSpeedPercent(lastValveCmd,  DAC2_PIN);
  Serial.println("Apply fan=" + String(lastFanSpeedCmd, 1) + "%, valve=" + String(lastValveCmd, 1) + "%");
}

// ----------------- Sensors -----------------
float readTemperature() {
  if (!adsInitialized) {
    Serial.println("Skipping temperature reading: ADS1115 not initialized.");
    return NAN;
  }
  int16_t adc0 = ads.readADC_SingleEnded(0);
  float volt_adc = adc0 * 4.096f / 32767.0f; // gain = 1 -> 4.096 V full-scale
  volt_adc = constrain(volt_adc, 0.001f, VCC - 0.001f); // avoid div by zero
  float r_ntc = (volt_adc * R_FIXED) / (VCC - volt_adc);
  float tempK = 1.0f / ((1.0f / T25) + (1.0f / BETA) * log(r_ntc / R25));
  return tempK - 273.15f;
}

// ----------------- Telemetry (one PATCH) -----------------
void sendAllDataToFirebase() {
  // Always read sensors, even if offline
    
  float temp = readTemperature();
  int dpState1 = digitalRead(DP1_SENSOR_PIN);
  int dpState2 = digitalRead(DP2_SENSOR_PIN);

 

 // Always declare and assign safe values first
float percent1 = 0.0f;
float percent2 = 0.0f;

// Check hardware and try to read, but don't block if unavailable
if (adsInitialized) {
  int16_t adc1 = ads.readADC_SingleEnded(1);
  float voltage1 = adc1 * 0.125f / 1000.0f;
  percent1 = clamp01pct((voltage1 / 3.052f) * 100.0f);

  int16_t adc2 = ads.readADC_SingleEnded(2);
  float voltage2 = adc2 * 0.125f / 1000.0f;
  percent2 = clamp01pct((voltage2 / 2.886f) * 100.0f);
} else {
  Serial.println("Skipping pressure reading: ADS1115 not initialized.");
  // percent1 and percent2 stay at safe 0.0f
}
  // If Wi‑Fi is offline, just skip the PATCH (no blocking)
  if (WiFi.status() != WL_CONNECTED) {
    wifiNeedsReconnect = true;
    Serial.println("Offline: skipping Firebase PATCH this cycle.");
    return;
  }

  String json = "{";
  json += "\"runstatus\":"   + String(dpState1) + ",";
  json += "\"filter\":"      + String(dpState2) + ",";
  json += "\"temperature\":" + String(isnan(temp) ? 0 : temp, 2) + ",";
  json += "\"valve\":"       + String(percent1, 1) + ",";
  json += "\"speed\":"       + String(percent2, 1);
  json += "}";

  firebasePATCH("/ahu/ahu5/data/feedback", json);
  Serial.println("Sent feedback PATCH.");
}

// ----------------- GPIO init -----------------
void setupPins() {
  pinMode(AHU1_RELAY_PIN, OUTPUT);
  pinMode(AHU2_RELAY_PIN, OUTPUT);
  digitalWrite(AHU1_RELAY_PIN, LOW);
  digitalWrite(AHU2_RELAY_PIN, LOW);

  pinMode(DP1_SENSOR_PIN, INPUT_PULLUP);
  pinMode(DP2_SENSOR_PIN, INPUT_PULLUP);

  pinMode(Red_RGB, OUTPUT);
  pinMode(Green_RGB, OUTPUT);
  pinMode(Blue_RGB, OUTPUT);
  setRGB(false, false, false);
}

// ----------------- Setup / Loop -----------------
void setup() {
  Serial.begin(115200);
  SPIFFS.begin(true);

  setupPins();
  setupWiFiManager();

  Wire.begin(SDA_PIN, SCL_PIN);
  adsInitialized = ads.begin();
  if (!adsInitialized) {
    Serial.println("Failed to initialize ADS1115.");
  } else {
    ads.setGain(GAIN_ONE);
  }

  // Apply initial LKG outputs at boot (stay at defaults until first valid cloud cmd)
  digitalWrite(AHU1_RELAY_PIN, lastRelayStatus);
  setPumpSpeedPercent(lastFanSpeedCmd, DAC1_PIN);
  setPumpSpeedPercent(lastValveCmd, DAC2_PIN);
}

void loop() {
  unsigned long currentMillis = millis();

  // Keep Wi‑Fi alive in the background (non-blocking)
  wifiMaintain();

  // Try to (re)initialize ADS if it was missing
  if (!adsInitialized) {
    adsInitialized = ads.begin();
    if (adsInitialized) {
      Serial.println("ADS1115 initialized successfully.");
      ads.setGain(GAIN_ONE);
    }
  }

  // Sensing/actuation cycle (fixed period)
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // ---- NEW ORDER ----
    // 1) Fetch all commands in one GET (updates caches if data present)
    receiveAllCommandsFromFirebase();

    // 2) Apply outputs from caches (stable even if the GET failed)
    controlAHURelays();
    controlFanSpeed();

    // 3) Telemetry up (best effort; skipped when offline)
    sendAllDataToFirebase();
  }
}