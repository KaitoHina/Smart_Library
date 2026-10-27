#include <Arduino.h>
#include "GP2Y0A21YK0F.h"

GP2Y0A21YK0F::GP2Y0A21YK0F(const int pin) : analogPin(pin)
{
}

float GP2Y0A21YK0F::getDistance()
{
  // 1. Read the analog value from the sensor
  int analogValue = analogRead(analogPin);

  // 2. Convert the analog value to voltage (assuming a 5V reference)
  float voltage = analogValue * (5.0 / 1023.0);

  // 3. Check for out-of-range conditions
  if (voltage <= 0.42) {
    return -1;  // voltage too low, distance is out of range (less than 10 cm)
  }
  
  // 4. Calculate the distance using the derived formula
  // Convert voltage to distance (in cm) based on the sensor's characteristic curve
  // Using the approximate formula: distance (cm) = 27.86 / (voltage - 0.42)
  // Note: This formula is only applicable to the GP2Y0A21YK0F and is valid within the 10cm to 80cm range
  float distance = 27.86 / (voltage - 0.42);
  
  // 5. Limit the output range to 10-80 cm
  distance = constrain(distance, 10.0, 80.0);

  // 6. Return the calculated distance
  return distance;
}