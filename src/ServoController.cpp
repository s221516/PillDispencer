// src/ServoController.cpp
#include "ServoController.h"
#include "PiezoController.h"
#include "Displayer.h"

ServoController::ServoController() 
    : servos{ServoMotor(Config::SERVO_PINS[0]), ServoMotor(Config::SERVO_PINS[1])},
      piezoSensor(nullptr),
      angle(Config::DEFAULT_ANGLE), 
      startAngle(Config::DEFAULT_START_ANGLE), 
      counter(0), 
      atStart(true) {
}

void ServoController::initialize() {
    for (int i = 0; i < Config::NUM_SERVOS; i++) {
        servos[i].initialize();
        servos[i].moveTo(startAngle);
    }
}

void ServoController::moveServo(int servoIndex, int targetAngle) {
    if (servoIndex >= 0 && servoIndex < Config::NUM_SERVOS) {
        servos[servoIndex].moveTo(targetAngle);
    }
}

void ServoController::resetAllServos() {
    for (int i = 0; i < Config::NUM_SERVOS; i++) {
        servos[i].reset();
    }
}

void ServoController::setAngle(int newAngle) {
    if (newAngle >= 0 && newAngle <= 180) {
        angle = newAngle;
    }
}

void ServoController::setStartAngle(int newStartAngle) {
    if (newStartAngle >= 0 && newStartAngle <= 180) {
        startAngle = newStartAngle;
        for (int i = 0; i < Config::NUM_SERVOS; i++) {
            servos[i].moveTo(startAngle);
        }
    }
}

void ServoController::setPiezoSensor(PiezoSensor* piezoController) {
    this->piezoSensor = piezoController;
}

bool ServoController::Dispense(int servoIndex, int maxAttempts) {
    if (servoIndex < 0 || servoIndex >= Config::NUM_SERVOS || !piezoSensor) {
        return false;
    }
    
    // Set which servo is currently dispensing for pattern analysis
    piezoSensor->setCurrentServo(servoIndex);
    
    for (int attempt = 1; attempt <= maxAttempts; attempt++) {
        
        piezoSensor->startTask();
        SemaphoreHandle_t readySemaphore = piezoSensor->getReadySemaphore();
        xSemaphoreTake(readySemaphore, portMAX_DELAY);

        Displayer::getInstance().logMessage("[SERVO] Attempt " + String(attempt) + "/" + String(maxAttempts));
        // Determine which position to move to based on current position
        int currentPos = servos[servoIndex].getCurrentAngle();
        int targetPos = (currentPos == startAngle) ? angle : startAngle;
        
        // Single movement per attempt
        servos[servoIndex].moveTo(targetPos);
        
        // Start the 1-second timeout timer for piezo detection
        piezoSensor->startTimeout();
        
        xSemaphoreTake(piezoSensor->getFinishedSemaphore(), portMAX_DELAY);
        if (piezoSensor->isTriggered()) {
            piezoSensor->setIsPillDrop(false);
            return true;
        } 
    }

    return false;
}