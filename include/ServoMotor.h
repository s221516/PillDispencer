// include/ServoMotor.h
#pragma once
#include <Arduino.h>
#include "Config.h"

class ServoMotor {
public:
    ServoMotor(int servoPin);
    void initialize();
    void moveTo(int targetAngle);
    void reset();
    int getCurrentAngle() const { return currentAngle; }
    
private:
    int pin;
    int currentAngle;
    int pwmChannel;
    static int channelCounter;  // Static counter for PWM channels
    
    void moveAndRelease(int target);
    uint32_t angleToLEDC(int angle);
};
