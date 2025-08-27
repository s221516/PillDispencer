// src/PiezoController.cpp
#include "PiezoController.h"

PiezoSensor::PiezoSensor()
    : isPillDrop(false),
      logCallback(nullptr),
      piezoMeasurements(Config::PIEZO_MEASUREMENTS),
      piezoTaskHandle(NULL),
      timeoutActive(false),
      timeoutStart(0)
{
    // Initialize array
    for (int i = 0; i < Config::NUM_PIEZOS; i++) {
        values[i] = "";
    }

    readySemaphore = xSemaphoreCreateBinary();
    finishedSemaphore = xSemaphoreCreateBinary();
}

void PiezoSensor::initialize() {
    pinMode(Config::LED_PIN, OUTPUT);
    for (int i = 0; i < Config::NUM_PIEZOS; i++) {
        pinMode(Config::PIEZO_PINS[i], INPUT);
    }
}

void PiezoSensor::startTask() {
    xTaskCreate(piezoTaskWrapper, "Piezo Read", Config::TASK_STACK_SIZE, this, 1, &piezoTaskHandle);
}

void PiezoSensor::startTimeout() {
    timeoutActive = true;
    timeoutStart = xTaskGetTickCount();
}

void PiezoSensor::piezoTaskWrapper(void* parameter) {
    PiezoSensor* controller = static_cast<PiezoSensor*>(parameter);
    controller->piezoTask();
}

void PiezoSensor::piezoTask() {
    isPillDrop = false;

    xSemaphoreGive(readySemaphore);
    while (true) {
        for (int i = 0; i < Config::NUM_PIEZOS; i++) {
            int val = analogRead(Config::PIEZO_PINS[i]);

            if (val > Config::PIEZO_THRESHOLD) {
                isPillDrop = true;
                digitalWrite(Config::LED_PIN, HIGH);
                startRecording(i, val); 
                
                // Reset timeout after successful detection
                timeoutActive = false;
                xSemaphoreGive(finishedSemaphore);  // Signal completion BEFORE delete
                piezoTaskHandle = NULL;
                vTaskDelete(NULL); // finished work, exit
            }
        }

        // Check timeout only if timeout is active
        if (timeoutActive && (xTaskGetTickCount() - timeoutStart > pdMS_TO_TICKS(Config::TASK_TIMEOUT_MS))) {
            // Timeout occurred - reset and signal
            timeoutActive = false;
            digitalWrite(Config::LED_PIN, LOW);  // Turn off LED
            xSemaphoreGive(finishedSemaphore);
            piezoTaskHandle = NULL;
            vTaskDelete(NULL); // finished work, exit
        }

        vTaskDelay(pdMS_TO_TICKS(5)); // yield, avoid busy loop
    }
}

void PiezoSensor::startRecording(int channel, int firstVal) {
    TickType_t startTime = xTaskGetTickCount(); // Record start time
    
    // Record the first value for the triggering channel
    values[channel] = String(firstVal);
    
    // Record initial values for all other channels
    for (int i = 0; i < Config::NUM_PIEZOS; i++) {
        if (i != channel) {
            int val = analogRead(Config::PIEZO_PINS[i]);
            values[i] = String(val);
        }
    }
    
    // Record subsequent measurements from all channels as fast as possible
    for (int j = 0; j < piezoMeasurements; j++) {
        for (int i = 0; i < Config::NUM_PIEZOS; i++) {
            int val = analogRead(Config::PIEZO_PINS[i]);
            values[i] += ", " + String(val);
        }
    }
    
    
    // Log all channels
    for (int i = 0; i < Config::NUM_PIEZOS; i++) {
        String msg = String("[PIEZO] ") + Config::PIEZO_NAMES[i] + " -> (";
        msg += values[i] + ")";
                
        if (logCallback) {
            logCallback(msg);
        }
    }

    // Calculate elapsed time and ensure total time is at least 1000ms
    TickType_t elapsedTime = xTaskGetTickCount() - startTime;
    TickType_t targetTime = pdMS_TO_TICKS(Config::TASK_TIMEOUT_MS*0.8);
    
    if (elapsedTime < targetTime) {
        vTaskDelay(targetTime - elapsedTime); // Sleep for remaining time
    }


}
