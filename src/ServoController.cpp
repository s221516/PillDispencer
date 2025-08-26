// src/ServoController.cpp
#include "ServoController.h"
#include "PiezoController.h"
#include "Displayer.h"

ServoController::ServoController() 
    : servos{ServoMotor(Config::SERVO_PINS[0]), ServoMotor(Config::SERVO_PINS[1])},
      piezoController(nullptr),
      angle(Config::DEFAULT_ANGLE), 
      startAngle(Config::DEFAULT_START_ANGLE), 
      counter(0), 
      atStart(true) {
    // Create mutex for thread-safe dispensing
    dispenseMutex = xSemaphoreCreateMutex();
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

void ServoController::toggle() {
    counter++;
    if (atStart) {
        // Move all servos to angle position
        for (int i = 0; i < Config::NUM_SERVOS; i++) {
            servos[i].moveTo(angle);
        }
        atStart = false;
    } else {
        // Move all servos to start position
        for (int i = 0; i < Config::NUM_SERVOS; i++) {
            servos[i].moveTo(startAngle);
        }
        atStart = true;
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

void ServoController::setPiezoController(PiezoController* piezoController) {
    this->piezoController = piezoController;
}

bool ServoController::fastDispenseWithFeedback(int servoIndex, int maxAttempts) {
    if (servoIndex < 0 || servoIndex >= Config::NUM_SERVOS || !piezoController) {
        return false;
    }
    
    // Wait for exclusive access to dispensing system
    if (xSemaphoreTake(dispenseMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        Displayer::getInstance().logMessage("[ERR] Could not acquire dispensing lock for servo " + String(servoIndex + 1));
        return false;
    }
    
    Displayer::getInstance().logMessage("[SERVO] Fast dispensing from servo " + String(servoIndex + 1) + " (max " + String(maxAttempts) + " attempts)");
    
    // Start piezo monitoring for the entire dispensing session
    piezoController->startMonitoring();
    
    for (int attempt = 1; attempt <= maxAttempts; attempt++) {
        Displayer::getInstance().logMessage("[SERVO] Attempt " + String(attempt) + "/" + String(maxAttempts));
        
        // Determine which position to move to based on current position
        int currentPos = servos[servoIndex].getCurrentAngle();
        int targetPos = (currentPos == startAngle) ? angle : startAngle;
        
        // Single movement per attempt
        servos[servoIndex].moveTo(targetPos);
        
        // Wait for drop signal during movement and after (total time for servo movement + pill drop)
        if (piezoController->waitForDropSignal(1000)) {
            // Pill detected! Wait for it to settle
            piezoController->stopMonitoring();
            Displayer::getInstance().logMessage("[SUCCESS] Fast dispense completed in " + String(attempt) + " attempts");
            
            // Release the mutex before returning
            xSemaphoreGive(dispenseMutex);
            return true;
        }
        
        // Brief pause before next attempt
        delay(100);  // Reduced delay between attempts
    }
    
    // Failed - stop monitoring and report
    piezoController->stopMonitoring();
    Displayer::getInstance().logMessage("[FAILED] Fast dispense failed after " + String(maxAttempts) + " attempts");
    
    // Release the mutex before returning
    xSemaphoreGive(dispenseMutex);
    return false;
}
