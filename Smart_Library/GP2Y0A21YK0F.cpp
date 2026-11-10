#include <Arduino.h>
#include "GP2Y0A21YK0F.h"

GP2Y0A21YK0F::GP2Y0A21YK0F(const int pin) : analogPin(pin)
{
}

int GP2Y0A21YK0F::readRaw()
{
  return analogRead(analogPin);
}

float GP2Y0A21YK0F::distanceFromRaw(int analogValue)
{
  // Convert the analog value to voltage (assuming a 5V reference)
  float voltage = analogValue * (5.0 / 1023.0);

  // Check for out-of-range conditions
  if (voltage <= 0.42) {
    return -1;  // voltage too low, distance is out of range (less than 10 cm)
  }

  // Calculate the distance using the derived formula
  // distance (cm) = 27.86 / (voltage - 0.42), valid within the 10cm to 80cm range
  float distance = 27.86 / (voltage - 0.42);

  // Limit the output range to 10-80 cm
  distance = constrain(distance, 10.0, 80.0);

  return distance;
}

float GP2Y0A21YK0F::getDistance()
{
  return distanceFromRaw(readRaw());
}
