// LD2410 raw UART test. Bypasses MyLD2410 to see if any bytes arrive at all.
// Radar TX must be on D17 (RX2). Open Serial Monitor at 115200.
//
//   bytes=0            -> wiring / power / wrong pin
//   F4 F3 F2 F1 ...    -> UART works, problem is in the library side
//   bytes at 115200    -> module was reconfigured, use 115200 in the sketch

void setup() {
  Serial.begin(115200);
  Serial.println(F("LD2410 raw UART test"));
}

void loop() {
  const unsigned long bauds[] = {256000, 115200};

  for (byte i = 0; i < 2; i++) {
    Serial2.end();
    Serial2.begin(bauds[i]);

    unsigned long start = millis();
    unsigned int count = 0;

    Serial.print(F("baud "));
    Serial.print(bauds[i]);
    Serial.print(F(": "));

    while (millis() - start < 2000) {
      while (Serial2.available()) {
        byte b = Serial2.read();
        if (count < 24) {
          if (b < 0x10) Serial.print('0');
          Serial.print(b, HEX);
          Serial.print(' ');
        }
        count++;
      }
    }

    Serial.print(F(" bytes="));
    Serial.println(count);
  }
}
