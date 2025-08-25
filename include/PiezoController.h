// include/PiezoController.h
#pragma once
#include <Arduino.h>
#include "Config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

class PiezoController {
public:
    typedef void (*LogCallback)(const String& msg);
    
    PiezoController();
    void initialize();
    void startTask();
    bool isTriggered() const { return piezoTriggered; }
    void setLogCallback(LogCallback logCallback) { this->logCallback = logCallback; }
    
    // Inter-thread communication methods
    void startMonitoring();  // Wake up piezo task to start monitoring
    void stopMonitoring();   // Stop monitoring
    bool waitForDropSignal(unsigned long timeoutMs = 1500);  // Wait for drop signal
    void resetDropDetection();

private:
    volatile bool piezoTriggered;
    volatile bool dropDetected;
    volatile unsigned long lastDropTime;
    bool inWave[Config::NUM_PIEZOS];
    String values[Config::NUM_PIEZOS];
    LogCallback logCallback;
    
    // FreeRTOS synchronization
    SemaphoreHandle_t dropSemaphore;      // Signals when a drop is detected
    volatile bool isMonitoring;           // Flag to control monitoring state
    TaskHandle_t piezoTaskHandle;         // Handle to the piezo task
    
    static void piezoTaskWrapper(void* parameter);
    void piezoTask();
};