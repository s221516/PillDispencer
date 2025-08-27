// src/PatternAnalyzer.cpp
#include "PatternAnalyzer.h"
#include "Config.h"
#include <algorithm>
#include <numeric>
#include <cmath>

// Initialize static threshold variables
float PatternAnalyzer::DEVIATION_THRESHOLD = 0.75f;     // Default average threshold
float PatternAnalyzer::MIN_CHANNEL_THRESHOLD = 0.6f;    // Default individual channel threshold

PatternAnalyzer::PatternAnalyzer() {
    for (int i = 0; i < Config::NUM_SERVOS; i++) {
        recordings[i].reserve(MAX_RECORDINGS);
        hasReference[i] = false;
        failedDispenses[i] = 0;
        
        // Initialize reference pattern with correct number of channels
        referencePattern[i].channelEnvelopes.resize(Config::NUM_PIEZOS);
    }
    
    // Load any saved progress from previous sessions
    loadAllProgress();
}

void PatternAnalyzer::setLogCallback(std::function<void(String)> callback) {
    logCallback = callback;
}

SignalEnvelope PatternAnalyzer::createEnvelope(const std::vector<int>& rawData, int targetPoints) {
    SignalEnvelope envelope;
    
    if (rawData.empty()) return envelope;
    
    // Calculate window size for envelope detection
    int windowSize = std::max(1, (int)rawData.size() / targetPoints);
    envelope.envelope.reserve(targetPoints);
    
    // Create envelope by taking max in each window
    for (int i = 0; i < targetPoints; i++) {
        int startIdx = (i * rawData.size()) / targetPoints;
        int endIdx = ((i + 1) * rawData.size()) / targetPoints;
        
        int maxInWindow = rawData[startIdx];
        for (int j = startIdx; j < endIdx && j < rawData.size(); j++) {
            maxInWindow = std::max(maxInWindow, rawData[j]);
        }
        envelope.envelope.push_back((float)maxInWindow);
    }
    
    // Calculate features
    envelope.maxValue = *std::max_element(envelope.envelope.begin(), envelope.envelope.end());
    envelope.totalArea = std::accumulate(envelope.envelope.begin(), envelope.envelope.end(), 0.0f);
    
    // Find peak index
    auto maxIt = std::max_element(envelope.envelope.begin(), envelope.envelope.end());
    envelope.peakIndex = std::distance(envelope.envelope.begin(), maxIt);
    
    envelope.timestamp = millis();
    
    return envelope;
}

float PatternAnalyzer::calculateSimilarity(const SignalEnvelope& env1, const SignalEnvelope& env2) const {
    if (env1.envelope.size() != env2.envelope.size()) return 0.0f;
    
    // Normalize envelopes for comparison
    std::vector<float> norm1(env1.envelope.size());
    std::vector<float> norm2(env2.envelope.size());
    
    float max1 = env1.maxValue > 0 ? env1.maxValue : 1.0f;
    float max2 = env2.maxValue > 0 ? env2.maxValue : 1.0f;
    
    for (size_t i = 0; i < env1.envelope.size(); i++) {
        norm1[i] = env1.envelope[i] / max1;
        norm2[i] = env2.envelope[i] / max2;
    }
    
    // Calculate Pearson correlation coefficient
    float sum1 = 0, sum2 = 0, sum1_sq = 0, sum2_sq = 0, sum_prod = 0;
    int n = norm1.size();
    
    for (int i = 0; i < n; i++) {
        sum1 += norm1[i];
        sum2 += norm2[i];
        sum1_sq += norm1[i] * norm1[i];
        sum2_sq += norm2[i] * norm2[i];
        sum_prod += norm1[i] * norm2[i];
    }
    
    float numerator = n * sum_prod - sum1 * sum2;
    float denominator = sqrt((n * sum1_sq - sum1 * sum1) * (n * sum2_sq - sum2 * sum2));
    
    if (denominator == 0) return 0.0f;
    
    float correlation = numerator / denominator;
    return std::max(0.0f, correlation); // Return positive correlation only
}

bool PatternAnalyzer::analyzeDispensing(
                                        int servoIndex, 
                                        const std::vector<std::vector<int>>& channelData, 
                                       String triggerChannel) {
    if (servoIndex >= Config::NUM_SERVOS || channelData.size() != Config::NUM_PIEZOS) return false;
    
    // Create envelopes for all channels
    DispensingRecord record;
    record.channelEnvelopes.resize(Config::NUM_PIEZOS);
    
    for (int i = 0; i < Config::NUM_PIEZOS; i++) {
        record.channelEnvelopes[i] = createEnvelope(channelData[i]);
        record.channelEnvelopes[i].triggerChannel = triggerChannel;
    }
    
    record.timestamp = millis();
    record.isValid = true;
    
    auto& servoRecordings = recordings[servoIndex];
    
    // LEARNING PHASE: Collect first 8 dispenses
    if (servoRecordings.size() < MAX_RECORDINGS - 1) {
        servoRecordings.push_back(record);
        
        // Save progress immediately after each recording
        saveServoProgress(servoIndex);
        
        if (logCallback) {
            logCallback("[PATTERN] Learning phase: " + String(servoRecordings.size()) + 
                       "/" + String(MAX_RECORDINGS) + " recordings collected (trigger: " + triggerChannel + ")");
        }
        return true; // Always return "normal" during learning
    }
    
    // BUILD MODEL: On 9th recording, build reference pattern and analyze it
    if (servoRecordings.size() == MAX_RECORDINGS - 1 && !hasReference[servoIndex]) {
        servoRecordings.push_back(record); // Add the 9th recording
        
        if (logCallback) {
            logCallback("[PATTERN] Learning complete! Building reference model from " + 
                       String(MAX_RECORDINGS) + " recordings...");
        }
        
        buildReferenceFromMajority(servoIndex);
        
        // Save complete model immediately
        saveServoProgress(servoIndex);
        
        // Now analyze the 9th recording against the newly built model
        // Fall through to analysis phase below
    }
    
    // ANALYSIS PHASE: Compare against model (dispense 11+)
    if (hasReference[servoIndex]) {
        float totalSimilarity = 0.0f;
        bool allChannelsGood = true;
        float lowestChannelSim = 1.0f;
        String problematicChannel = "";
        
        // Calculate similarity for all channels
        for (int i = 0; i < Config::NUM_PIEZOS; i++) {
            float channelSim = calculateSimilarity(record.channelEnvelopes[i], 
                                                 referencePattern[servoIndex].channelEnvelopes[i]);
            totalSimilarity += channelSim;
            
            // Check individual channel threshold
            if (channelSim < MIN_CHANNEL_THRESHOLD) {
                allChannelsGood = false;
                if (channelSim < lowestChannelSim) {
                    lowestChannelSim = channelSim;
                    problematicChannel = String(Config::PIEZO_NAMES[i]);
                }
            }
        }
        
        float avgSimilarity = totalSimilarity / Config::NUM_PIEZOS;
        bool avgGood = avgSimilarity >= DEVIATION_THRESHOLD;
        
        // BEST-OF-BOTH APPROACH: Accept if either the average is good OR any single channel is excellent
        // This handles cases where pill drops to one side and primarily hits one sensor
        float maxChannelSim = 0.0f;
        String bestChannel = "";
        for (int i = 0; i < Config::NUM_PIEZOS; i++) {
            float channelSim = calculateSimilarity(record.channelEnvelopes[i], 
                                                 referencePattern[servoIndex].channelEnvelopes[i]);
            if (channelSim > maxChannelSim) {
                maxChannelSim = channelSim;
                bestChannel = String(Config::PIEZO_NAMES[i]);
            }
        }
        
        bool bestChannelExcellent = maxChannelSim >= DEVIATION_THRESHOLD;
        bool isNormal = avgGood || bestChannelExcellent;  // Accept if EITHER condition is met
        
        if (logCallback) {
            String simDetails = "Similarities: ";
            for (int i = 0; i < Config::NUM_PIEZOS; i++) {
                if (i > 0) simDetails += ", ";
                float channelSim = calculateSimilarity(record.channelEnvelopes[i], 
                                                     referencePattern[servoIndex].channelEnvelopes[i]);
                simDetails += String(Config::PIEZO_NAMES[i]) + ": " + String(channelSim, 3);
            }
            
            String reasonStr = "";
            if (!avgGood && !bestChannelExcellent) {
                reasonStr += "Both average (" + String(avgSimilarity, 3) + " < " + String(DEVIATION_THRESHOLD, 2) + 
                           ") and best channel " + bestChannel + " (" + String(maxChannelSim, 3) + " < " + String(DEVIATION_THRESHOLD, 2) + ") below threshold";
            }
            
            logCallback("[PATTERN] Avg similarity: " + String(avgSimilarity, 3) + 
                       ", Best: " + bestChannel + " " + String(maxChannelSim, 3) + 
                       " (" + simDetails + ") - " + (isNormal ? "NORMAL" : "ABNORMAL"));
            
            if (isNormal && !avgGood && bestChannelExcellent) {
                logCallback("[PATTERN] Accepted via best-of-both: " + bestChannel + " sensor shows good similarity");
            }
            
            if (!isNormal) {
                logCallback("[PATTERN] Rejection reason: " + reasonStr);
            }
        }
        
        if (!isNormal) {
            failedDispenses[servoIndex]++;
            if (logCallback) {
                logCallback("[PATTERN] FLAWED DISPENSE detected! Total failed: " + 
                           String(failedDispenses[servoIndex]));
            }
        }
        
        return isNormal;
    }
    
    // Should not reach here, but return true as fallback
    return true;
}

bool PatternAnalyzer::buildReferenceFromMajority(int servoIndex) {
    auto& servoRecordings = recordings[servoIndex];
    if (servoRecordings.size() < MAX_RECORDINGS) return false;
    
    // Calculate similarity matrix
    std::vector<std::vector<float>> similarityMatrix(servoRecordings.size());
    for (size_t i = 0; i < servoRecordings.size(); i++) {
        similarityMatrix[i].resize(servoRecordings.size());
        for (size_t j = 0; j < servoRecordings.size(); j++) {
            if (i == j) {
                similarityMatrix[i][j] = 1.0f;
            } else {
                float totalSim = 0.0f;
                
                // Calculate similarity across all channels
                for (int ch = 0; ch < Config::NUM_PIEZOS; ch++) {
                    float channelSim = calculateSimilarity(servoRecordings[i].channelEnvelopes[ch], 
                                                         servoRecordings[j].channelEnvelopes[ch]);
                    totalSim += channelSim;
                }
                
                similarityMatrix[i][j] = totalSim / Config::NUM_PIEZOS;
            }
        }
    }
    
    // Find the largest group of similar recordings
    std::vector<bool> used(servoRecordings.size(), false);
    std::vector<int> bestGroup;
    
    for (size_t i = 0; i < servoRecordings.size(); i++) {
        if (used[i]) continue;
        
        std::vector<int> group;
        group.push_back(i);
        used[i] = true;
        
        for (size_t j = i + 1; j < servoRecordings.size(); j++) {
            if (used[j]) continue;
            
            // Check if j is similar to all members of current group
            bool similar = true;
            for (int groupMember : group) {
                if (similarityMatrix[groupMember][j] < SIMILARITY_THRESHOLD) {
                    similar = false;
                    break;
                }
            }
            
            if (similar) {
                group.push_back(j);
                used[j] = true;
            }
        }
        
        if (group.size() > bestGroup.size()) {
            bestGroup = group;
        }
    }
    
    // Need at least majority of the recordings to be similar to build reliable reference
    int minSimilarRecordings = (MAX_RECORDINGS + 1) / 2; // 5 out of 9 (true majority)
    if (bestGroup.size() < minSimilarRecordings) {
        if (logCallback) {
            logCallback("[PATTERN] Not enough similar recordings to build reference (found " + 
                       String(bestGroup.size()) + ", need " + String(minSimilarRecordings) + "+)");
        }
        return false;
    }
    
    // Create reference pattern by averaging the best group
    for (int ch = 0; ch < Config::NUM_PIEZOS; ch++) {
        referencePattern[servoIndex].channelEnvelopes[ch].envelope.clear();
        
        int envelopeSize = servoRecordings[bestGroup[0]].channelEnvelopes[ch].envelope.size();
        referencePattern[servoIndex].channelEnvelopes[ch].envelope.resize(envelopeSize, 0.0f);
        
        // Average the envelopes for this channel
        for (int idx : bestGroup) {
            for (int i = 0; i < envelopeSize; i++) {
                referencePattern[servoIndex].channelEnvelopes[ch].envelope[i] += 
                    servoRecordings[idx].channelEnvelopes[ch].envelope[i];
            }
        }
        
        for (int i = 0; i < envelopeSize; i++) {
            referencePattern[servoIndex].channelEnvelopes[ch].envelope[i] /= bestGroup.size();
        }
        
        // Calculate reference features for this channel
        auto& channelRef = referencePattern[servoIndex].channelEnvelopes[ch];
        channelRef.maxValue = *std::max_element(channelRef.envelope.begin(), channelRef.envelope.end());
        channelRef.totalArea = std::accumulate(channelRef.envelope.begin(), channelRef.envelope.end(), 0.0f);
        
        auto maxIt = std::max_element(channelRef.envelope.begin(), channelRef.envelope.end());
        channelRef.peakIndex = std::distance(channelRef.envelope.begin(), maxIt);
    }
    
    hasReference[servoIndex] = true;
    
    if (logCallback) {
        logCallback("[PATTERN] Built reference pattern for servo " + String(servoIndex + 1) + 
                   " from " + String(bestGroup.size()) + "/" + String(servoRecordings.size()) + 
                   " recordings");
        logCallback("[PATTERN] Reference quality: " + String(getReferenceQuality(servoIndex), 3));
    }
    
    return true;
}

int PatternAnalyzer::getFailedCount(int servoIndex) const {
    return servoIndex < Config::NUM_SERVOS ? failedDispenses[servoIndex] : 0;
}

int PatternAnalyzer::getRecordingCount(int servoIndex) const {
    return servoIndex < Config::NUM_SERVOS ? recordings[servoIndex].size() : 0;
}

float PatternAnalyzer::getReferenceQuality(int servoIndex) const {
    if (servoIndex >= Config::NUM_SERVOS || !hasReference[servoIndex]) return 0.0f;
    
    // Calculate average similarity of all recordings to reference across all channels
    float totalSimilarity = 0.0f;
    int count = 0;
    
    for (const auto& record : recordings[servoIndex]) {
        float recordSimilarity = 0.0f;
        
        for (int ch = 0; ch < Config::NUM_PIEZOS; ch++) {
            float channelSim = calculateSimilarity(record.channelEnvelopes[ch], 
                                                 referencePattern[servoIndex].channelEnvelopes[ch]);
            recordSimilarity += channelSim;
        }
        
        totalSimilarity += recordSimilarity / Config::NUM_PIEZOS;
        count++;
    }
    
    return count > 0 ? totalSimilarity / count : 0.0f;
}

String PatternAnalyzer::getAnalysisReport(int servoIndex) const {
    if (servoIndex >= Config::NUM_SERVOS) return "Invalid servo index";
    
    String report = "[ANALYSIS] Servo " + String(servoIndex + 1) + " Report:\n";
    report += "  Recordings: " + String(getRecordingCount(servoIndex)) + "/" + String(MAX_RECORDINGS) + "\n";
    report += "  Failed dispenses: " + String(getFailedCount(servoIndex)) + "\n";
    report += "  Has reference: " + String(hasReference[servoIndex] ? "Yes" : "No") + "\n";
    
    if (hasReference[servoIndex]) {
        report += "  Reference quality: " + String(getReferenceQuality(servoIndex), 3) + "\n";
    }
    
    return report;
}

void PatternAnalyzer::saveAllProgress() {
    if (logCallback) {
        logCallback("[PATTERN] Saving all learning progress to SPIFFS...");
    }
    
    for (int servoIndex = 0; servoIndex < Config::NUM_SERVOS; servoIndex++) {
        saveServoProgress(servoIndex);
    }
}

void PatternAnalyzer::loadAllProgress() {
    if (logCallback) {
        logCallback("[PATTERN] Loading all learning progress from SPIFFS...");
    }
    
    for (int servoIndex = 0; servoIndex < Config::NUM_SERVOS; servoIndex++) {
        loadServoProgress(servoIndex);
    }
}

void PatternAnalyzer::saveServoProgress(int servoIndex) {
    if (servoIndex >= Config::NUM_SERVOS) return;
    
    String filename = "/servo" + String(servoIndex) + "_progress.dat";
    File file = SPIFFS.open(filename, "w");
    
    if (!file) {
        if (logCallback) {
            logCallback("[PATTERN] Failed to create progress file: " + filename);
        }
        return;
    }
    
    auto& servoRecordings = recordings[servoIndex];
    
    // Write header: number of recordings, has reference flag
    size_t numRecordings = servoRecordings.size();
    file.write((uint8_t*)&numRecordings, sizeof(size_t));
    file.write((uint8_t*)&hasReference[servoIndex], sizeof(bool));
    file.write((uint8_t*)&failedDispenses[servoIndex], sizeof(int));
    
    // Write all recordings
    for (const auto& record : servoRecordings) {
        // Write timestamp and validity
        file.write((uint8_t*)&record.timestamp, sizeof(unsigned long));
        file.write((uint8_t*)&record.isValid, sizeof(bool));
        
        // Write all channel envelopes
        for (int ch = 0; ch < Config::NUM_PIEZOS; ch++) {
            const auto& envelope = record.channelEnvelopes[ch];
            
            // Write envelope size and data
            size_t envelopeSize = envelope.envelope.size();
            file.write((uint8_t*)&envelopeSize, sizeof(size_t));
            file.write((uint8_t*)envelope.envelope.data(), envelopeSize * sizeof(float));
            
            // Write envelope features
            file.write((uint8_t*)&envelope.maxValue, sizeof(float));
            file.write((uint8_t*)&envelope.totalArea, sizeof(float));
            file.write((uint8_t*)&envelope.peakIndex, sizeof(int));
            file.write((uint8_t*)&envelope.timestamp, sizeof(unsigned long));
            
            // Write trigger channel string
            uint8_t triggerLen = envelope.triggerChannel.length();
            file.write(&triggerLen, 1);
            file.write((uint8_t*)envelope.triggerChannel.c_str(), triggerLen);
        }
    }
    
    // Write reference pattern if it exists
    if (hasReference[servoIndex]) {
        for (int ch = 0; ch < Config::NUM_PIEZOS; ch++) {
            const auto& refEnvelope = referencePattern[servoIndex].channelEnvelopes[ch];
            
            // Write reference envelope
            size_t refSize = refEnvelope.envelope.size();
            file.write((uint8_t*)&refSize, sizeof(size_t));
            file.write((uint8_t*)refEnvelope.envelope.data(), refSize * sizeof(float));
            
            // Write reference features
            file.write((uint8_t*)&refEnvelope.maxValue, sizeof(float));
            file.write((uint8_t*)&refEnvelope.totalArea, sizeof(float));
            file.write((uint8_t*)&refEnvelope.peakIndex, sizeof(int));
        }
    }
    
    file.close();
    
    if (logCallback) {
        logCallback("[PATTERN] Saved servo " + String(servoIndex + 1) + " progress: " + 
                   String(servoRecordings.size()) + " recordings, model: " + 
                   (hasReference[servoIndex] ? "Yes" : "No"));
    }
}

bool PatternAnalyzer::loadServoProgress(int servoIndex) {
    if (servoIndex >= Config::NUM_SERVOS) return false;
    
    String filename = "/servo" + String(servoIndex) + "_progress.dat";
    File file = SPIFFS.open(filename, "r");
    
    if (!file) {
        if (logCallback) {
            logCallback("[PATTERN] No saved progress for servo " + String(servoIndex + 1));
        }
        return false;
    }
    
    auto& servoRecordings = recordings[servoIndex];
    servoRecordings.clear();
    
    // Read header
    size_t numRecordings;
    bool hasRef;
    int failedCount;
    
    file.read((uint8_t*)&numRecordings, sizeof(size_t));
    file.read((uint8_t*)&hasRef, sizeof(bool));
    file.read((uint8_t*)&failedCount, sizeof(int));
    
    hasReference[servoIndex] = hasRef;
    failedDispenses[servoIndex] = failedCount;
    
    // Read all recordings
    for (size_t r = 0; r < numRecordings; r++) {
        DispensingRecord record;
        record.channelEnvelopes.resize(Config::NUM_PIEZOS);
        
        // Read timestamp and validity
        file.read((uint8_t*)&record.timestamp, sizeof(unsigned long));
        file.read((uint8_t*)&record.isValid, sizeof(bool));
        
        // Read all channel envelopes
        for (int ch = 0; ch < Config::NUM_PIEZOS; ch++) {
            auto& envelope = record.channelEnvelopes[ch];
            
            // Read envelope size and data
            size_t envelopeSize;
            file.read((uint8_t*)&envelopeSize, sizeof(size_t));
            envelope.envelope.resize(envelopeSize);
            file.read((uint8_t*)envelope.envelope.data(), envelopeSize * sizeof(float));
            
            // Read envelope features
            file.read((uint8_t*)&envelope.maxValue, sizeof(float));
            file.read((uint8_t*)&envelope.totalArea, sizeof(float));
            file.read((uint8_t*)&envelope.peakIndex, sizeof(int));
            file.read((uint8_t*)&envelope.timestamp, sizeof(unsigned long));
            
            // Read trigger channel string
            uint8_t triggerLen;
            file.read(&triggerLen, 1);
            char triggerBuf[triggerLen + 1];
            file.read((uint8_t*)triggerBuf, triggerLen);
            triggerBuf[triggerLen] = '\0';
            envelope.triggerChannel = String(triggerBuf);
        }
        
        servoRecordings.push_back(record);
    }
    
    // Read reference pattern if it exists
    if (hasReference[servoIndex]) {
        referencePattern[servoIndex].channelEnvelopes.resize(Config::NUM_PIEZOS);
        
        for (int ch = 0; ch < Config::NUM_PIEZOS; ch++) {
            auto& refEnvelope = referencePattern[servoIndex].channelEnvelopes[ch];
            
            // Read reference envelope
            size_t refSize;
            file.read((uint8_t*)&refSize, sizeof(size_t));
            refEnvelope.envelope.resize(refSize);
            file.read((uint8_t*)refEnvelope.envelope.data(), refSize * sizeof(float));
            
            // Read reference features
            file.read((uint8_t*)&refEnvelope.maxValue, sizeof(float));
            file.read((uint8_t*)&refEnvelope.totalArea, sizeof(float));
            file.read((uint8_t*)&refEnvelope.peakIndex, sizeof(int));
        }
    }
    
    file.close();
    
    if (logCallback) {
        logCallback("[PATTERN] Loaded servo " + String(servoIndex + 1) + " progress: " + 
                   String(servoRecordings.size()) + " recordings, model: " + 
                   (hasReference[servoIndex] ? "Yes" : "No"));
    }
    
    return true;
}

void PatternAnalyzer::resetServoData(int servoIndex) {
    if (servoIndex >= Config::NUM_SERVOS) return;
    
    // Clear all in-memory data
    recordings[servoIndex].clear();
    hasReference[servoIndex] = false;
    failedDispenses[servoIndex] = 0;
    
    // Clear reference pattern
    referencePattern[servoIndex].channelEnvelopes.clear();
    referencePattern[servoIndex].channelEnvelopes.resize(Config::NUM_PIEZOS);
    
    // Delete saved file
    String filename = "/servo" + String(servoIndex) + "_progress.dat";
    if (SPIFFS.exists(filename)) {
        SPIFFS.remove(filename);
    }
    
    if (logCallback) {
        logCallback("[PATTERN] RESET: All data cleared for servo " + String(servoIndex + 1));
    }
}

void PatternAnalyzer::resetAllData() {
    if (logCallback) {
        logCallback("[PATTERN] RESET: Clearing all data for all servos...");
    }
    
    for (int servoIndex = 0; servoIndex < Config::NUM_SERVOS; servoIndex++) {
        resetServoData(servoIndex);
    }
    
    if (logCallback) {
        logCallback("[PATTERN] RESET: All servo data has been cleared");
    }
}

void PatternAnalyzer::setDeviationThreshold(float threshold) {
    if (threshold >= 0.0f && threshold <= 1.0f) {
        DEVIATION_THRESHOLD = threshold;
        if (logCallback) {
            logCallback("[PATTERN] Average similarity threshold set to: " + String(threshold, 3));
        }
    } else {
        if (logCallback) {
            logCallback("[PATTERN] Invalid threshold: " + String(threshold, 3) + " (must be 0.0-1.0)");
        }
    }
}

void PatternAnalyzer::setMinChannelThreshold(float threshold) {
    if (threshold >= 0.0f && threshold <= 1.0f) {
        MIN_CHANNEL_THRESHOLD = threshold;
        if (logCallback) {
            logCallback("[PATTERN] Individual channel threshold set to: " + String(threshold, 3));
        }
    } else {
        if (logCallback) {
            logCallback("[PATTERN] Invalid threshold: " + String(threshold, 3) + " (must be 0.0-1.0)");
        }
    }
}

float PatternAnalyzer::getDeviationThreshold() const {
    return DEVIATION_THRESHOLD;
}

float PatternAnalyzer::getMinChannelThreshold() const {
    return MIN_CHANNEL_THRESHOLD;
}
