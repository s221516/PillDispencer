// include/PiezoController.h
#pragma once
#include <Arduino.h>
#include "Config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

class PiezoSensor {
public:
    typedef void (*LogCallback)(const String& msg);
    
    PiezoSensor();
    void initialize();
    void startTask();
    bool isTriggered() const { return isPillDrop; }
    void setLogCallback(LogCallback logCallback) { this->logCallback = logCallback; }
    void startRecording(int channel, int firstVal);
    void setReadySemaphore(SemaphoreHandle_t semaphore) { readySemaphore = semaphore; }
    SemaphoreHandle_t getReadySemaphore() const { return readySemaphore; }
    SemaphoreHandle_t getFinishedSemaphore() const { return finishedSemaphore; }
    void setIsPillDrop(bool value) { isPillDrop = value; }
    void startTimeout(); // Start the 1-second timeout timer
    void setPiezoMeasurements(int measurements) { piezoMeasurements = measurements; }
    int getPiezoMeasurements() const { return piezoMeasurements; }

private:
    volatile bool isPillDrop;
    int piezoMeasurements;
    String values[Config::NUM_PIEZOS];
    LogCallback logCallback;
    
    // Timeout control
    volatile bool timeoutActive;
    volatile TickType_t timeoutStart;
    
    // FreeRTOS synchronization
    SemaphoreHandle_t readySemaphore;      // Signals when a drop is detected
    SemaphoreHandle_t finishedSemaphore;
    TaskHandle_t piezoTaskHandle;         // Handle to the piezo task
    
    
    static void piezoTaskWrapper(void* parameter);
    void piezoTask();
};