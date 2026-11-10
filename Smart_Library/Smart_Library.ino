#include "GP2Y0A21YK0F.h"

// Change these pins to match the actual Mega 2560 wiring.
constexpr uint8_t SEAT_A_PIN = A0;
constexpr uint8_t SEAT_B_PIN = A1;
constexpr uint8_t SEAT_C_PIN = A2;
constexpr uint8_t SEAT_D_PIN = A3;
constexpr uint8_t MIC_PIN = A4;
constexpr uint8_t RADAR_OUT_PIN = 26;

// Create an array of GP2Y0A21YK0F objects for the four IR sensors
GP2Y0A21YK0F irSensors[4] = {
  GP2Y0A21YK0F(SEAT_A_PIN), GP2Y0A21YK0F(SEAT_B_PIN),
  GP2Y0A21YK0F(SEAT_C_PIN), GP2Y0A21YK0F(SEAT_D_PIN)
};

void setup() {
  Serial.begin(115200);
  pinMode(RADAR_OUT_PIN, INPUT);
}

void loop() {
  static unsigned long lastReport = 0;
  const unsigned long now = millis();
  if (now - lastReport < 500) return;
  lastReport = now;

  // Measure microphone peak-to-peak amplitude over a 50 ms window.
  analogRead(MIC_PIN); // Let the ADC settle after switching from the seat inputs.
  int low = 1023;
  int high = 0;
  const unsigned long started = millis();
  while (millis() - started < 50) {
    const int sample = analogRead(MIC_PIN);
    if (sample < low) low = sample;
    if (sample > high) high = sample;
  }

  Serial.print(F("{\"distanceCm\":{"));
  for (byte seat = 0; seat < 4; ++seat) {
    if (seat != 0) Serial.print(',');
    Serial.print('"');
    Serial.print(char('A' + seat));
    Serial.print(F("\":"));
    irSensors[seat].getDistance(); // Discard first conversion after switching inputs.
    const float distance = irSensors[seat].getDistance();
    if (distance < 0) Serial.print(F("null"));
    else Serial.print(distance, 1);
  }
  Serial.print(F("},\"noiseAmplitude\":"));
  Serial.print(high - low);
  Serial.print(F(",\"radar\":"));
  Serial.print(digitalRead(RADAR_OUT_PIN) == HIGH ? 1 : 0);
  Serial.println('}');
}
