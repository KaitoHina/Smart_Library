#include "GP2Y0A21YK0F.h"

// Create an array of GP2Y0A21YK0F objects for the four IR sensors
GP2Y0A21YK0F irSensors[4] = {
  GP2Y0A21YK0F(A0), GP2Y0A21YK0F(A1), 
  GP2Y0A21YK0F(A2), GP2Y0A21YK0F(A3)
};

void setup() {
  Serial.begin(115200);
}

void loop() {
  
}