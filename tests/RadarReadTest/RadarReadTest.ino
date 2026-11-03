#include <MyLD2410.h>

const uint8_t RADAR_OUT_PIN = 26;   // radar OUT: HIGH = presence
// Serial2 = D16 (TX2) / D17 (RX2)
MyLD2410 radar(Serial2);

// The OUT pin is driven by the radar itself, so it keeps working even when
// the UART frame is missed or the loop is busy.
bool radarOutPresence() {
  return digitalRead(RADAR_OUT_PIN) == HIGH;
}

void setup() {
  Serial.begin(115200);
  pinMode(RADAR_OUT_PIN, INPUT);

  // 38400, not 256000: the Mega's 16 MHz UART runs 256000 at 250000 (-2.3%),
  // which corrupts the last data bit. 38400 is accurate (0.16%).
  Serial2.begin(38400);
  if (!radar.begin()) {
    Serial.println(F("LD2410 not responding"));
  }
  radar.setMaxGate(4, 4, 5);        // 4 gates = 3 m, hold presence 5 s
}

void loop() {
  radar.check();

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint < 250) return;
  lastPrint = millis();

  Serial.print(F("OUT="));
  Serial.print(radarOutPresence() ? 1 : 0);
  Serial.print(F(" uart="));
  Serial.print(radar.presenceDetected() ? 1 : 0);
  Serial.print(F(" status="));
  Serial.print(radar.getStatus());
  Serial.print(F(" dist="));
  Serial.print(radar.detectedDistance());
  Serial.println();
}
