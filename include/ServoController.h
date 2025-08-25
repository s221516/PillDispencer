// include/ServoController.h
#pragma once
#include <Arduino.h>
#include "ServoMotor.h"
#include "Config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class PiezoController; // Forward declaration

class ServoController {
public:
    ServoController();
    void initialize();
    void setPiezoController(PiezoController* piezoController);
    void moveServo(int servoIndex, int targetAngle);
    bool fastDispenseWithFeedback(int servoIndex, int maxAttempts = 20);
    void resetAllServos();
    void toggle();
    void setAngle(int newAngle);
    void setStartAngle(int newStartAngle);
    int getAngle() const { return angle; }
    int getStartAngle() const { return startAngle; }
    int getCounter() const { return counter; }
    bool isAtStart() const { return atStart; }
    void resetCounter() { counter = 0; }

private:
    ServoMotor servos[Config::NUM_SERVOS];
    PiezoController* piezoController;
    int angle;
    int startAngle;
    int counter;
    bool atStart;
    
    // Mutex to prevent concurrent dispensing operations
    SemaphoreHandle_t dispenseMutex;
};
