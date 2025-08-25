// src/ServoMotor.cpp
#include "ServoMotor.h"

ServoMotor::ServoMotor(int servoPin) : pin(servoPin), currentAngle(0) {
    static int nextChannel = 0;
    pwmChannel = nextChannel++;
}

void ServoMotor::initialize() {
    // Initial position will be set by the manager
}

void ServoMotor::moveAndRelease(int target) {
    servo.attach(pin);
    delay(10); // Let PWM stabilize
    servo.write(target);
    delay(Config::SERVO_DELAY_MS);
    servo.detach();
    delay(10); // Let PWM deallocate cleanly
    currentAngle = target;
}

void ServoMotor::moveTo(int targetAngle) {
    if (targetAngle >= 0 && targetAngle <= 180) {
        moveAndRelease(targetAngle);
    }
}

void ServoMotor::reset() {
    moveAndRelease(180);
}
