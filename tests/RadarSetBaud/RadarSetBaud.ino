// One-time: move the LD2410 from 256000 to 38400 baud.
//
// The Mega's 16 MHz UART cannot generate 256000 exactly: it runs at 250000
// (-2.3%). That makes the receiver sample the last data bit too late, so every
// byte with a 0 MSB is read as 0x80. The header/tail survive because their MSB
// is already 1, which is why the frames look "almost right".
//
// 38400 is accurate on the Mega (0.16% error) and the radar supports it.
// Run this once, then change the main sketch to Serial2.begin(38400).
// If it fails, power-cycle the radar and run it again.

void send(const uint8_t *frame, uint8_t n) {
  Serial2.write(frame, n);
  Serial2.flush();
  delay(200);
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(256000);
  delay(2000);

  Serial.println(F("Setting radar baud to 38400..."));

  // config enable, set baud index 3 (38400), reboot
  const uint8_t enable[]  = {0xFD,0xFC,0xFB,0xFA, 0x04,0x00, 0xFF,0x00, 0x01,0x00, 0x04,0x03,0x02,0x01};
  const uint8_t setBaud[] = {0xFD,0xFC,0xFB,0xFA, 0x04,0x00, 0xA1,0x00, 0x03,0x00, 0x04,0x03,0x02,0x01};
  const uint8_t reboot[]  = {0xFD,0xFC,0xFB,0xFA, 0x02,0x00, 0xA3,0x00, 0x04,0x03,0x02,0x01};

  send(enable, sizeof(enable));
  send(setBaud, sizeof(setBaud));
  send(reboot, sizeof(reboot));

  delay(1000);
  Serial2.end();
  Serial2.begin(38400);
  delay(1000);

  Serial.println(F("Listening at 38400 for 5 s..."));
  unsigned long start = millis();
  unsigned int count = 0;
  while (millis() - start < 5000) {
    while (Serial2.available()) {
      uint8_t b = Serial2.read();
      if (count < 32) {
        if (b < 0x10) Serial.print('0');
        Serial.print(b, HEX);
        Serial.print(' ');
      }
      count++;
    }
  }

  Serial.print(F("\nbytes="));
  Serial.println(count);
  Serial.println(count ? F("OK: change the main sketch to 38400")
                       : F("FAIL: power-cycle the radar and run again"));
}

void loop() {}
