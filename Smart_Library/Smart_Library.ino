/*
  Smart Library - seat availability + noise monitoring (Arduino Mega 2560)
  4 IR seat sensors (analog), analog mic, mmWave presence (OUT pin), SSD1306 OLED,
  RGB LED, seat LEDs, buzzer, 2 buttons. Sensor timing uses millis(); WiFiEsp
  network requests are synchronous.

  Telemetry is sent to the dashboard over Wi-Fi every second.
  The Mega controls an ESP8266 running AT firmware through Serial1; set
  WIFI_TELEMETRY to 0 to print JSON over USB instead. Legacy fields remain.
  Telemetry includes seat, noise, mode and radar state.
*/
#define WIFI_TELEMETRY 1 // 1 = WiFiEsp POST, 0 = USB JSON output

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdio.h>
#if WIFI_TELEMETRY
#include <WiFiEsp.h>
#endif

#include "GP2Y0A21YK0F.h"
#include "MyLD2410.h"

// =====================  CONFIG  =====================
#define DEMO_TIMING 0          // 1 = shorter seat debounce for quick demos
#define SERIAL_CSV  0          // 1 = also print one CSV line per second for test data
#if WIFI_TELEMETRY
const char* const WIFI_SSID = "EE3070_P1615_1";
const char* const WIFI_PASSWORD = "EE3070P1615";
const char* const DASHBOARD_HOST = "192.168.1.23"; // PC LAN IPv4 address
const uint16_t DASHBOARD_PORT = 8080;
const unsigned long ESP_AT_BAUD = 115200; // Must match the ESP8266 AT firmware
const unsigned long WIFI_RETRY_MS = 15000;
const unsigned long WIFI_CHECK_MS = 5000;
#endif

// ---- pins ----
constexpr uint8_t NUM_SEATS = 4;
constexpr uint8_t SEAT_A_PIN = A1;  // IR sensor analog input
constexpr uint8_t SEAT_B_PIN = A2;  // IR sensor analog input
constexpr uint8_t SEAT_C_PIN = A3;  // IR sensor analog input
constexpr uint8_t SEAT_D_PIN = A4;  // IR sensor analog input
constexpr uint8_t SEAT_PIN[NUM_SEATS] = { SEAT_A_PIN, SEAT_B_PIN, SEAT_C_PIN, SEAT_D_PIN };
constexpr uint8_t SEAT_LED_PIN[NUM_SEATS] = { 27, 28, 29, 30 };  // per-seat LEDs (via resistor)
constexpr uint8_t MMWAVE_OUT_PIN = 26;                           // radar OUT (HIGH = presence)
constexpr uint8_t MIC_PIN = A0;                                  // mic module AO
constexpr uint8_t BLUE_PIN = 2, GREEN_PIN = 3, RED_PIN = 4;      // platform RGB (transistor driven, HIGH = on)
constexpr uint8_t BUZZER_PIN = 5;                                // active buzzer
constexpr uint8_t MODE_BTN = 31, ACK_BTN = 32;                   // INPUT_PULLUP, pressed = LOW

// ---- sensor tuning ----
const int SEAT_THRESHOLD = 400;             // Raw ADC threshold (0-1023). Adjust for your mounting distance!
const bool SEAT_LED_ON_WHEN_FREE = true;    // seat LED lit = seat available

// ---- timing (ms) ----
#if DEMO_TIMING
const unsigned long OCC_CONFIRM_MS = 1000, FREE_CONFIRM_MS = 2000;
#else
const unsigned long OCC_CONFIRM_MS = 3000, FREE_CONFIRM_MS = 10000;
#endif
const unsigned long NOISE_WINDOW_MS = 100;
const unsigned long NOISE_RISING_MS = 3000;    // show "rising" (yellow)
const unsigned long NOISE_WARN_MS   = 5000;    // red LED + OLED notice
const unsigned long NOISE_BEEP_MS   = 15000;   // log + one short beep
const unsigned long BEEP_LEN_MS     = 150;
const unsigned long DISAGREE_MS     = 5000;    // disagreement must persist this long
const unsigned long RADAR_WARMUP_MS = 10000;   // ignore radar after power-up
const unsigned long OLED_PERIOD_MS  = 250;
const unsigned long TELEMETRY_MS    = 1000;

// ---- noise thresholds (peak-to-peak ADC counts) - CALIBRATE THESE ----
// index: 0 = Silent, 1 = Quiet, 2 = Collaborative
const uint16_t TH_MODERATE[3] = { 30,  60,  120 };
const uint16_t TH_NOISY[3]    = { 100, 160, 280 };
const uint16_t HYST = 8;                       // hysteresis when dropping to a lower level
const uint8_t  AVG_WINDOWS = 8;                // moving average length

const uint8_t NOISE_QUIET = 0, NOISE_MODERATE = 1, NOISE_NOISY = 2;
const char* const MODE_NAME[3]  = { "SILENT", "QUIET", "COLLAB" };
const char* const LEVEL_NAME[3] = { "QUIET", "MODERATE", "NOISY" };

// =====================  STATE  =====================
Adafruit_SSD1306 display(128, 64, &Wire, -1);
bool displayReady = false;

// Create an array of GP2Y0A21YK0F objects for the four IR sensors
GP2Y0A21YK0F irSensors[NUM_SEATS] = {
  GP2Y0A21YK0F(SEAT_PIN[0]), GP2Y0A21YK0F(SEAT_PIN[1]),
  GP2Y0A21YK0F(SEAT_PIN[2]), GP2Y0A21YK0F(SEAT_PIN[3])
};

// seats
bool seatOcc[NUM_SEATS];
bool seatPending[NUM_SEATS];
unsigned long seatChangeStart[NUM_SEATS];
float seatDistance[NUM_SEATS];
uint8_t freeCount = NUM_SEATS;

// noise
int  nMin = 1023, nMax = 0;
unsigned long nWinStart = 0;
uint16_t ampRing[AVG_WINDOWS];
uint8_t  ampIdx = 0;
uint16_t noiseAvg = 0;
uint8_t  noiseLevel = NOISE_QUIET;
bool     noiseEpisode = false;
unsigned long noiseStart = 0;
bool     beepDone = false, acked = false;
uint8_t  noiseStage = 0;          // 0 none, 1 rising, 2 warning
bool     warningActive = false;

// presence / fusion
bool zonePresent = false;
bool disagreeRaw = false, disagree = false;
unsigned long disagreeStart = 0;

// ui / outputs
uint8_t zoneMode = 1;             // start in Quiet mode
unsigned long beepStart = 0; bool beeping = false;
unsigned long lastOled = 0, lastCsv = 0, lastTelemetry = 0;

// buttons
const uint8_t BTN_PIN[2] = { MODE_BTN, ACK_BTN };
bool btnLast[2] = { true, true }, btnStable[2] = { true, true };
bool btnEvent[2] = { false, false };
unsigned long btnChanged[2] = { 0, 0 };

String telemetryJson;
#if WIFI_TELEMETRY
WiFiEspClient telemetryClient;
bool wifiConnected = false, wifiAttempted = false, postFailed = false;
unsigned long lastWifiAttempt = 0, lastWifiCheck = 0;
#endif

// =====================  HELPERS  =====================
char seatId(uint8_t i) {
  return char('A' + i);
}

void appendJsonBool(String& json, bool value) {
  json += value ? F("true") : F("false");
}

void setRGB(bool r, bool g, bool b) {
  digitalWrite(RED_PIN, r);
  digitalWrite(GREEN_PIN, g);
  digitalWrite(BLUE_PIN, b);
}

uint8_t occupiedCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < NUM_SEATS; i++) if (seatOcc[i]) n++;
  return n;
}

void resetNoiseWindow() {
  nMin = 1023;
  nMax = 0;
  nWinStart = millis();
}

// =====================  SEATS  =====================
void updateSeats(unsigned long now) {
  for (uint8_t i = 0; i < NUM_SEATS; i++) {
    const int rawValue = irSensors[i].readRaw();
    seatDistance[i] = irSensors[i].distanceFromRaw(rawValue);
    const bool raw = rawValue > SEAT_THRESHOLD;

    if (raw != seatOcc[i]) {
      if (!seatPending[i]) { seatPending[i] = true; seatChangeStart[i] = now; }
      unsigned long need = raw ? OCC_CONFIRM_MS : FREE_CONFIRM_MS;   // fast to occupy, slow to free
      if (now - seatChangeStart[i] >= need) {
        seatOcc[i] = raw;
        seatPending[i] = false;
        Serial.print(F("EVENT seat ")); Serial.print(i + 1);
        Serial.println(raw ? F(" OCCUPIED") : F(" FREE"));
      }
    } else {
      seatPending[i] = false;              // raw agrees with confirmed state: cancel
    }
  }
  freeCount = NUM_SEATS - occupiedCount();
}

// =====================  NOISE  =====================
void updateNoise(unsigned long now) {
  int v = analogRead(MIC_PIN);
  if (v < nMin) nMin = v;
  if (v > nMax) nMax = v;
  if (now - nWinStart < NOISE_WINDOW_MS) return;

  // end of a 100 ms window: peak-to-peak amplitude -> moving average
  ampRing[ampIdx] = (uint16_t)(nMax - nMin);
  ampIdx = (ampIdx + 1) % AVG_WINDOWS;
  uint32_t sum = 0;
  for (uint8_t i = 0; i < AVG_WINDOWS; i++) sum += ampRing[i];
  noiseAvg = sum / AVG_WINDOWS;
  nMin = 1023; nMax = 0; nWinStart = now;

  // classify with hysteresis
  uint16_t mod = TH_MODERATE[zoneMode], noisy = TH_NOISY[zoneMode];
  uint8_t lvl;
  if (noiseAvg >= noisy || (noiseLevel == NOISE_NOISY && noiseAvg + HYST >= noisy)) lvl = NOISE_NOISY;
  else if (noiseAvg >= mod || (noiseLevel >= NOISE_MODERATE && noiseAvg + HYST >= mod)) lvl = NOISE_MODERATE;
  else lvl = NOISE_QUIET;

  // episode tracking (duration filter)
  if (lvl == NOISE_NOISY && !noiseEpisode) {
    noiseEpisode = true; noiseStart = now; beepDone = false; acked = false;
  } else if (lvl != NOISE_NOISY && noiseEpisode) {
    noiseEpisode = false;
    if (noiseStage >= 2) {
      Serial.print(F("EVENT noise episode ended after ms: ")); Serial.println(now - noiseStart);
    }
  }
  noiseLevel = lvl;
}

void updateNoiseStage(unsigned long now) {
  noiseStage = 0;
  if (noiseEpisode) {
    unsigned long d = now - noiseStart;
    if (d >= NOISE_WARN_MS) noiseStage = 2;
    else if (d >= NOISE_RISING_MS) noiseStage = 1;
    if (d >= NOISE_BEEP_MS && !beepDone) {
      beepDone = true;
      if (!acked) { beeping = true; beepStart = now; }
      Serial.println(F("EVENT sustained noise (15 s)"));
    }
  }
  warningActive = (noiseStage >= 2) && !acked;
}

// =====================  mmWave + FUSION  =====================
void updatePresenceAndFusion(unsigned long now) {
  zonePresent = digitalRead(MMWAVE_OUT_PIN);
  bool occ = occupiedCount() > 0;
  bool warm = now > RADAR_WARMUP_MS;
  disagreeRaw = warm && (occ != zonePresent);   // seats vs radar disagree
  if (disagreeRaw) {
    if (disagreeStart == 0) disagreeStart = now;
    if (now - disagreeStart >= DISAGREE_MS) {
      if (!disagree) Serial.println(occ ? F("EVENT fusion: seats occupied, no radar presence")
                                        : F("EVENT fusion: radar presence, all seats free"));
      disagree = true;
    }
  } else {
    disagreeStart = 0;
    disagree = false;
  }
}

// =====================  BUTTONS  =====================
void updateButtons(unsigned long now) {
  for (uint8_t i = 0; i < 2; i++) {
    bool r = digitalRead(BTN_PIN[i]);
    if (r != btnLast[i]) { btnLast[i] = r; btnChanged[i] = now; }
    if (now - btnChanged[i] >= 30 && r != btnStable[i]) {
      btnStable[i] = r;
      if (!r) btnEvent[i] = true;              // pressed (active LOW)
    }
  }
  if (btnEvent[0]) {
    btnEvent[0] = false;
    zoneMode = (zoneMode + 1) % 3;
    Serial.print(F("EVENT mode -> ")); Serial.println(MODE_NAME[zoneMode]);
  }
  if (btnEvent[1]) {
    btnEvent[1] = false;
    if (noiseEpisode) { acked = true; Serial.println(F("EVENT warning acknowledged")); }
  }
}

// =====================  OUTPUTS  =====================
void updateOutputs(unsigned long now) {
  // seat LEDs
  for (uint8_t i = 0; i < NUM_SEATS; i++)
    digitalWrite(SEAT_LED_PIN[i], SEAT_LED_ON_WHEN_FREE ? !seatOcc[i] : seatOcc[i]);

  // RGB priority: disagreement > sustained noise > rising/moderate > full > normal
  bool effModerate = (noiseLevel == NOISE_MODERATE) || (noiseLevel == NOISE_NOISY && noiseStage < 2) || acked;
  if (disagree)            { bool on = (now / 500) % 2; setRGB(on, on, false); }   // flashing yellow
  else if (warningActive)  setRGB(true, false, false);                              // red
  else if (effModerate && noiseLevel != NOISE_QUIET) setRGB(true, true, false);     // yellow
  else if (freeCount == 0) setRGB(true, false, true);                               // purple
  else                     setRGB(false, true, false);                              // green

  // buzzer: one short non-blocking beep
  if (beeping && now - beepStart >= BEEP_LEN_MS) beeping = false;
  digitalWrite(BUZZER_PIN, beeping);
}

// =====================  OLED  =====================
void drawSeatMap() {
  display.setCursor(92, 0); display.print(F("SEATS"));
  for (uint8_t i = 0; i < NUM_SEATS; i++) {
    int x = 94 + (i % 2) * 18, y = 14 + (i / 2) * 22;
    if (seatOcc[i]) {
      display.fillRect(x, y, 16, 18, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.drawRect(x, y, 16, 18, SSD1306_WHITE);
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(x + 5, y + 5); display.print(i + 1);
    display.setTextColor(SSD1306_WHITE);
  }
}

void updateDisplay(unsigned long now) {
  if (!displayReady) return;
  if (now - lastOled < OLED_PERIOD_MS) return;
  lastOled = now;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (disagree) {
    display.setCursor(0, 0);  display.println(F("SENSOR CHECK"));
    display.setCursor(0, 16); display.print(F("Seat map: ")); display.print(occupiedCount()); display.println(F(" occ"));
    display.print(F("mmWave: ")); display.println(zonePresent ? F("presence") : F("none"));
    display.setCursor(0, 44); display.println(zonePresent ? F("Untracked activity?") : F("Check radar/calib."));
  } else if (warningActive) {
    display.setCursor(0, 0);  display.println(F("QUIET ZONE NOTICE"));
    display.setCursor(0, 20); display.println(F("Noise is high."));
    display.println(F("Please lower your"));
    display.println(F("voice."));
  } else if (freeCount == 0) {
    display.setCursor(0, 0);  display.println(F("ZONE A FULL"));
    display.setCursor(0, 20); display.println(F("Free seats: 0 / 4"));
    display.println(F("Please check"));
    display.println(F("another zone."));
  } else {
    display.setCursor(0, 0);  display.print(F("SMART LIBRARY"));
    display.setCursor(0, 14); display.print(F("Mode: "));  display.print(MODE_NAME[zoneMode]);
    display.setCursor(0, 26); display.print(F("Free: "));  display.print(freeCount); display.print(F(" / ")); display.print(NUM_SEATS);
    display.setCursor(0, 38); display.print(F("Noise: ")); display.print(LEVEL_NAME[noiseLevel]);
    display.setCursor(0, 50); display.print(F("Presence: ")); display.print(zonePresent ? F("YES") : F("NO"));
    drawSeatMap();
  }
  display.display();
}

// =====================  LOGGING  =====================
void logCsv(unsigned long now) {
#if SERIAL_CSV
  if (now - lastCsv < 1000) return;
  lastCsv = now;
  // ms, seat bits (1=occupied), free, noiseAvg, level, stage, presence, mode
  Serial.print(F("CSV,")); Serial.print(now); Serial.print(',');
  for (uint8_t i = 0; i < NUM_SEATS; i++) Serial.print(seatOcc[i] ? '1' : '0');
  Serial.print(','); Serial.print(freeCount);
  Serial.print(','); Serial.print(noiseAvg);
  Serial.print(','); Serial.print(noiseLevel);
  Serial.print(','); Serial.print(noiseStage);
  Serial.print(','); Serial.print(zonePresent);
  Serial.print(','); Serial.println(zoneMode);
#endif
}

void buildTelemetryJson() {
  telemetryJson = F("{\"distanceCm\":{");
  for (uint8_t i = 0; i < NUM_SEATS; i++) {
    if (i != 0) telemetryJson += ',';
    telemetryJson += '"'; telemetryJson += seatId(i); telemetryJson += F("\":");
    if (seatDistance[i] < 0) telemetryJson += F("null");
    else telemetryJson += String(seatDistance[i], 1);
  }

  telemetryJson += F("},\"seatOccupied\":{");
  for (uint8_t i = 0; i < NUM_SEATS; i++) {
    if (i != 0) telemetryJson += ',';
    telemetryJson += '"'; telemetryJson += seatId(i); telemetryJson += F("\":");
    appendJsonBool(telemetryJson, seatOcc[i]);
  }

  telemetryJson += F("},\"freeCount\":");
  telemetryJson += String((unsigned int)freeCount);
  telemetryJson += F(",\"noiseAmplitude\":");
  telemetryJson += String((unsigned int)noiseAvg);
  telemetryJson += F(",\"noiseLevel\":\"");
  telemetryJson += LEVEL_NAME[noiseLevel];
  telemetryJson += F("\",\"noiseStage\":");
  telemetryJson += String((unsigned int)noiseStage);
  telemetryJson += F(",\"noiseWarning\":");
  appendJsonBool(telemetryJson, warningActive);
  telemetryJson += F(",\"mode\":\"");
  telemetryJson += MODE_NAME[zoneMode];
  telemetryJson += F("\",\"radar\":");
  telemetryJson += zonePresent ? '1' : '0';
  telemetryJson += F(",\"disagree\":");
  appendJsonBool(telemetryJson, disagree);
  telemetryJson += '}';
}

#if WIFI_TELEMETRY
void updateWiFi(unsigned long now) {
  if (wifiConnected) {
    if (now - lastWifiCheck < WIFI_CHECK_MS) return;
    lastWifiCheck = now;
    wifiConnected = WiFi.status() == WL_CONNECTED;
    if (wifiConnected) return;
    Serial.println(F("EVENT WiFi disconnected"));
  }

  if (wifiAttempted && now - lastWifiAttempt < WIFI_RETRY_MS) return;
  wifiAttempted = true;
  lastWifiAttempt = now;
  Serial.println(F("EVENT WiFi connecting"));
  wifiConnected = WiFi.begin(WIFI_SSID, WIFI_PASSWORD) == WL_CONNECTED;
  Serial.println(wifiConnected ? F("EVENT WiFi connected") : F("EVENT WiFi connect failed"));
  resetNoiseWindow();
}

bool postTelemetry(const String& payload) {
  if (!wifiConnected) return false;
  if (!telemetryClient.connect(DASHBOARD_HOST, DASHBOARD_PORT)) {
    resetNoiseWindow();
    return false;
  }

  char header[192];
  const int headerLength = snprintf(
    header, sizeof(header),
    "POST /api/telemetry HTTP/1.1\r\n"
    "Host: %s:%u\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: %u\r\n"
    "Connection: close\r\n\r\n",
    DASHBOARD_HOST,
    (unsigned int)DASHBOARD_PORT,
    (unsigned int)payload.length()
  );
  if (headerLength < 0 || (size_t)headerLength >= sizeof(header)) {
    telemetryClient.stop();
    return false;
  }

  const size_t sentHeader = telemetryClient.write((const uint8_t*)header, (size_t)headerLength);
  if (sentHeader != (size_t)headerLength) {
    telemetryClient.stop();
    resetNoiseWindow();
    return false;
  }
  const size_t sentBody = telemetryClient.write((const uint8_t*)payload.c_str(), payload.length());
  telemetryClient.stop();
  resetNoiseWindow();
  return sentHeader == (size_t)headerLength && sentBody == payload.length();
}
#endif

void logTelemetry(unsigned long now) {
  if (now - lastTelemetry < TELEMETRY_MS) return;
  lastTelemetry = now;
  buildTelemetryJson();
#if WIFI_TELEMETRY
  const bool sent = postTelemetry(telemetryJson);
  if (sent && postFailed) Serial.println(F("EVENT dashboard POST restored"));
  else if (!sent && !postFailed) Serial.println(F("EVENT dashboard POST failed"));
  postFailed = !sent;
#else
  Serial.println(telemetryJson);
#endif
}

// =====================  SETUP / LOOP  =====================
void setup() {
  Serial.begin(115200);

  for (uint8_t i = 0; i < NUM_SEATS; i++) {
    pinMode(SEAT_LED_PIN[i], OUTPUT);
    seatDistance[i] = -1;
  }

  pinMode(MMWAVE_OUT_PIN, INPUT);
  pinMode(RED_PIN, OUTPUT); pinMode(GREEN_PIN, OUTPUT); pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MODE_BTN, INPUT_PULLUP); pinMode(ACK_BTN, INPUT_PULLUP);

  Wire.begin();
  Wire.setClock(400000);                       // faster OLED refresh -> less mic sampling gap
  displayReady = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!displayReady) {
    Serial.println(F("EVENT SSD1306 not found; continuing without display"));
  }

  for (uint8_t i = 0; i < NUM_SEATS; i++) {    // start from the real state (no 3 s wait)
    const int rawValue = irSensors[i].readRaw();
    seatOcc[i] = rawValue > SEAT_THRESHOLD;
    seatDistance[i] = irSensors[i].distanceFromRaw(rawValue);
    seatPending[i] = false;
  }
  freeCount = NUM_SEATS - occupiedCount();

  Serial.println(F("Smart Library started"));
#if WIFI_TELEMETRY
  telemetryJson.reserve(320);
  Serial1.begin(ESP_AT_BAUD);
  WiFi.init(&Serial1);
  if (WiFi.status() == WL_NO_SHIELD) Serial.println(F("EVENT ESP8266 AT module not responding"));
  else updateWiFi(millis());
#endif
  nWinStart = millis();
#if SERIAL_CSV
  Serial.println(F("CSV,ms,seats,free,noiseAvg,level,stage,presence,mode"));
#endif
}

void loop() {
  unsigned long now = millis();
  updateButtons(now);
  updateSeats(now);
  updateNoise(now);
  updateNoiseStage(now);
  updatePresenceAndFusion(now);
  updateOutputs(now);
  updateDisplay(now);
  logCsv(now);
#if WIFI_TELEMETRY
  updateWiFi(millis());
#endif
  logTelemetry(millis());
}
