// src/PiezoController.cpp
#include "PiezoController.h"

PiezoController::PiezoController() 
    : piezoTriggered(false), 
    dropDetected(false),
    lastDropTime(0),
    logCallback(nullptr),
    isMonitoring(false),
    piezoTaskHandle(nullptr) {
    for (int i = 0; i < Config::NUM_PIEZOS; i++) {
        inWave[i] = false;
        values[i] = "";
    }
    
    // Create semaphore for drop detection signaling
    dropSemaphore = xSemaphoreCreateBinary();
}

void PiezoController::initialize() {
    pinMode(Config::LED_PIN, OUTPUT);
    for (int i = 0; i < Config::NUM_PIEZOS; i++) {
        pinMode(Config::PIEZO_PINS[i], INPUT);
    }
}

void PiezoController::startTask() {
    xTaskCreate(piezoTaskWrapper, "Piezo Read", Config::TASK_STACK_SIZE, this, 1, &piezoTaskHandle);
}

void PiezoController::piezoTaskWrapper(void* parameter) {
    PiezoController* controller = static_cast<PiezoController*>(parameter);
    controller->piezoTask();
}

void PiezoController::piezoTask() {
    piezoTriggered = false;
    
    while (true) {
        // Only process when monitoring is active
        if (isMonitoring) {
            for (int i = 0; i < Config::NUM_PIEZOS; i++) {
                int val = analogRead(Config::PIEZO_PINS[i]);

                if (val > Config::PIEZO_THRESHOLD) {
                    piezoTriggered = true;
                    digitalWrite(Config::LED_PIN, HIGH);

                    if (!inWave[i]) {
                        inWave[i] = true;
                        values[i] = "";  // start new wave
                    }
                    values[i] += String(val) + ", ";

                } else {
                    if (inWave[i]) {
                        // Wave ended → log it once and signal drop
                        inWave[i] = false;
                        dropDetected = true;
                        lastDropTime = millis();

                        // Build message
                        String msg = String("[PIEZO] ") + Config::PIEZO_NAMES[i] + " -> (";
                        if (values[i].endsWith(", ")) {
                            values[i].remove(values[i].length() - 2); // trim last comma
                        }
                        msg += values[i] + ")";
                        
                        if (logCallback) {
                            logCallback(msg);
                        }
                        
                        // Signal that a drop was detected
                        xSemaphoreGive(dropSemaphore);
                        piezoTriggered = false;
                    }
                }
            }

            if (!piezoTriggered) {
                digitalWrite(Config::LED_PIN, LOW);
            }
            
            // Fast monitoring when active
            vTaskDelay(5 / portTICK_PERIOD_MS);
        } else {
            // Sleep longer when not monitoring
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
    }
}

void PiezoController::startMonitoring() {
    isMonitoring = true;
    // Clear any pending semaphore signals
    xSemaphoreTake(dropSemaphore, 0);
    dropDetected = false;
    
    if (logCallback) {
        logCallback("[PIEZO] Monitoring started");
    }
}

void PiezoController::stopMonitoring() {
    isMonitoring = false;
    
    if (logCallback) {
        logCallback("[PIEZO] Monitoring stopped");
    }
}

bool PiezoController::waitForDropSignal(unsigned long timeoutMs) {
    // Wait for semaphore signal from piezo task
    if (xSemaphoreTake(dropSemaphore, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {
        if (logCallback) {
            logCallback("[PIEZO] Drop signal received");
        }
        return true;
    }
    
    if (logCallback) {
        logCallback("[PIEZO] Timeout - no drop signal");
    }
    return false;
}

void PiezoController::resetDropDetection() {
    dropDetected = false;
    lastDropTime = 0;
}
