#ifndef GP2Y0A21YK0F_H
#define GP2Y0A21YK0F_H

class GP2Y0A21YK0F
{
private:
  const int analogPin;
public:
  GP2Y0A21YK0F(const int pin);

  float getDistance();
};

#endif // GP2Y0A21YK0F_H