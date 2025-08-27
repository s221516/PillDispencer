// include/PiezoController.h
#pragma once
#include <Arduino.h>
#include "Config.h"
#include "PatternAnalyzer.h"
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
    
    // Pattern analysis
    void setCurrentServo(int servoIndex) { currentServoIndex = servoIndex; }
    String getAnalysisReport(int servoIndex) const;
    int getFailedCount(int servoIndex) const;
    
    // Data management
    void resetServoData(int servoIndex);
    void resetAllData();
    
    // Threshold management
    void setDeviationThreshold(float threshold);
    void setMinChannelThreshold(float threshold);
    float getDeviationThreshold() const;
    float getMinChannelThreshold() const;
    
    // Command interface aliases
    bool setAverageThreshold(float threshold);
    bool setChannelThreshold(float threshold);
    float getAverageThreshold() const;
    float getChannelThreshold() const;

private:
    volatile bool isPillDrop;
    int piezoMeasurements;
    int currentServoIndex;
    String values[Config::NUM_PIEZOS];
    LogCallback logCallback;
    PatternAnalyzer patternAnalyzer;
    
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