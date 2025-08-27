// include/PatternAnalyzer.h
#pragma once

#include <Arduino.h>
#include <vector>
#include <functional>
#include "SPIFFS.h"
#include "Config.h"

struct SignalEnvelope {
    std::vector<float> envelope;
    float maxValue;
    float totalArea;
    int peakIndex;
    String triggerChannel;
    unsigned long timestamp;
};

struct DispensingRecord {
    std::vector<SignalEnvelope> channelEnvelopes; // Dynamic array for all channels
    bool isValid;
    float similarity;
    unsigned long timestamp;
    
    DispensingRecord() {
        channelEnvelopes.resize(Config::NUM_PIEZOS);
        isValid = false;
        similarity = 0.0f;
        timestamp = 0;
    }
};

class PatternAnalyzer {
private:
    static constexpr int MAX_RECORDINGS = 9; // Odd number for cleaner majority voting
    static constexpr float SIMILARITY_THRESHOLD = 0.7f;
    static float DEVIATION_THRESHOLD;      // Average similarity threshold (adjustable)
    static float MIN_CHANNEL_THRESHOLD;    // Minimum similarity for any individual channel (adjustable)
    static constexpr int ENVELOPE_POINTS = 50; // Reduced resolution for envelope
    
    std::vector<DispensingRecord> recordings[Config::NUM_SERVOS];
    DispensingRecord referencePattern[Config::NUM_SERVOS];
    bool hasReference[Config::NUM_SERVOS];
    int failedDispenses[Config::NUM_SERVOS];
    
    std::function<void(String)> logCallback;

public:
    PatternAnalyzer();
    
    void setLogCallback(std::function<void(String)> callback);
    
    // Main analysis function - now takes dynamic data
    bool analyzeDispensing(int servoIndex, const std::vector<std::vector<int>>& channelData, 
                          String triggerChannel);
    
    // Signal processing
    SignalEnvelope createEnvelope(const std::vector<int>& rawData, int targetPoints = ENVELOPE_POINTS);
    float calculateSimilarity(const SignalEnvelope& env1, const SignalEnvelope& env2) const;
    
    // Pattern management
    void updateReferencePattern(int servoIndex);
    bool buildReferenceFromMajority(int servoIndex);
    
    // Statistics
    int getFailedCount(int servoIndex) const;
    int getRecordingCount(int servoIndex) const;
    float getReferenceQuality(int servoIndex) const;
    
    // Data export for analysis
    String getAnalysisReport(int servoIndex) const;
    
    // Progress persistence - saves ALL learning progress
    void saveAllProgress();           // Save all recordings and models to SPIFFS
    void loadAllProgress();           // Load all recordings and models from SPIFFS
    void saveServoProgress(int servoIndex);  // Save progress for specific servo
    bool loadServoProgress(int servoIndex);  // Load progress for specific servo
    
    // Data management
    void resetServoData(int servoIndex);     // Reset all data for specific servo
    void resetAllData();                     // Reset all data for all servos
    
    // Threshold management (runtime adjustable)
    void setDeviationThreshold(float threshold);       // Set average similarity threshold
    void setMinChannelThreshold(float threshold);      // Set minimum individual channel threshold
    float getDeviationThreshold() const;               // Get current average threshold
    float getMinChannelThreshold() const;              // Get current individual channel threshold
    String exportRecordings(int servoIndex) const;
    
    // Model persistence
    bool saveReferenceModel(int servoIndex) const;
    bool loadReferenceModel(int servoIndex);
    void loadAllReferenceModels();
};
