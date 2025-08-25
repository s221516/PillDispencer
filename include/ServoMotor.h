// include/ServoMotor.h
#pragma once
#include <Arduino.h>
#include <ESP32Servo.h>
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
    Servo servo;
    
    void moveAndRelease(int target);
};
