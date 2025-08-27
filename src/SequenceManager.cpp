// src/SequenceManager.cpp
#include "SequenceManager.h"
#include "ServoController.h"
#include "Displayer.h"
#include <Preferences.h>

SequenceManager::SequenceManager(ServoController& servoController) 
    : servoController(servoController) {
}

void SequenceManager::initialize() {
    loadFromStorage();
    Displayer::getInstance().logMessage("[SEQ] Sequence Manager initialized");
}

bool SequenceManager::parseSequenceCommand(const String& command, String& deviceId, String& name, std::vector<int>& counts) {
    // Expected format: "SEQUENCE device123 morning (1,1,0,2,0,6)"
    if (!command.startsWith("SEQUENCE ")) {
        return false;
    }
    
    String params = command.substring(9); // Remove "SEQUENCE "
    int firstSpace = params.indexOf(' ');
    int openParen = params.indexOf('(');
    int closeParen = params.indexOf(')');
    
    if (firstSpace == -1 || openParen == -1 || closeParen == -1) {
        return false;
    }
    
    deviceId = params.substring(0, firstSpace);
    name = params.substring(firstSpace + 1, openParen);
    name.trim();
    
    String countsStr = params.substring(openParen + 1, closeParen);
    counts.clear();
    
    // Parse comma-separated numbers
    int startIdx = 0;
    for (int i = 0; i <= countsStr.length(); i++) {
        if (i == countsStr.length() || countsStr.charAt(i) == ',') {
            if (i > startIdx) {
                String numStr = countsStr.substring(startIdx, i);
                numStr.trim();
                counts.push_back(numStr.toInt());
            }
            startIdx = i + 1;
        }
    }
    
    // Ensure we have the right number of servos
    if (counts.size() != Config::NUM_SERVOS) {
        return false;
    }

    return true;
}

bool SequenceManager::storeSequence(const String& deviceId, const String& name, const std::vector<int>& counts) {
    PillSequence sequence;
    sequence.deviceId = deviceId;
    sequence.name = name;
    sequence.servoCounts = counts;
    sequence.timestamp = millis();
    
    // Store in memory
    deviceSequences[deviceId].push_back(sequence);
    
    // Save to persistent storage
    saveToStorage();
    
    Displayer::getInstance().logMessage("[SEQ] Stored sequence '" + name + "' for device " + deviceId);
    return true;
}

bool SequenceManager::executeSequence(const String& deviceId, const String& name) {
    auto deviceIter = deviceSequences.find(deviceId);
    if (deviceIter == deviceSequences.end()) {
        Displayer::getInstance().logMessage("[ERR] Device " + deviceId + " not found");
        return false;
    }
    
    for (const auto& sequence : deviceIter->second) {
        if (sequence.name == name) {
            Displayer::getInstance().logMessage("[SEQ] Executing sequence '" + name + "' for device " + deviceId);
            executeServoSequence(sequence.servoCounts);
            return true;
        }
    }
    
    Displayer::getInstance().logMessage("[ERR] Sequence '" + name + "' not found for device " + deviceId);
    return false;
}

void SequenceManager::executeServoSequence(const std::vector<int>& counts) {
    int totalPills = 0;
    int successfulPills = 0;
    int failedPills = 0;

    for (int servoIndex = 0; servoIndex < Config::NUM_SERVOS && servoIndex < counts.size(); servoIndex++) {
        int runCount = counts[servoIndex];
        
        for (int run = 0; run < runCount; run++) {
            totalPills++;
            Displayer::getInstance().logMessage("[SEQ] Dispensing pill " + String(totalPills) + " from servo " + String(servoIndex + 1) + " (run " + String(run + 1) + "/" + String(runCount) + ")");
            

            if (servoController.Dispense(servoIndex)) {
                successfulPills++;
            } else {
                failedPills++;
            }
        }
    }
    Displayer::getInstance().logMessage("[SEQ] Sequence complete: " + String(successfulPills) + " pills dispensed, " + String(failedPills) + " failures");
}

bool SequenceManager::deleteSequence(const String& deviceId, const String& name) {
    auto deviceIter = deviceSequences.find(deviceId);
    if (deviceIter == deviceSequences.end()) {
        return false;
    }
    
    auto& sequences = deviceIter->second;
    for (auto it = sequences.begin(); it != sequences.end(); ++it) {
        if (it->name == name) {
            sequences.erase(it);
            saveToStorage();
            Displayer::getInstance().logMessage("[SEQ] Deleted sequence '" + name + "' for device " + deviceId);
            return true;
        }
    }
    
    return false;
}

std::vector<String> SequenceManager::getSequenceNames(const String& deviceId) {
    std::vector<String> names;
    auto deviceIter = deviceSequences.find(deviceId);
    if (deviceIter != deviceSequences.end()) {
        for (const auto& sequence : deviceIter->second) {
            names.push_back(sequence.name);
        }
    }
    return names;
}

void SequenceManager::saveToStorage() {
    Preferences prefs;
    prefs.begin("sequences", false);
    
    // Clear existing data
    prefs.clear();
    
    int sequenceIndex = 0;
    for (const auto& devicePair : deviceSequences) {
        for (const auto& sequence : devicePair.second) {
            String key = "seq_" + String(sequenceIndex);
            
            // Create a serialized string: "deviceId|name|count1,count2,count3..."
            String data = sequence.deviceId + "|" + sequence.name + "|";
            for (size_t i = 0; i < sequence.servoCounts.size(); i++) {
                if (i > 0) data += ",";
                data += String(sequence.servoCounts[i]);
            }
            
            prefs.putString(key.c_str(), data);
            sequenceIndex++;
        }
    }
    
    prefs.putInt("count", sequenceIndex);
    prefs.end();
}

void SequenceManager::loadFromStorage() {
    Preferences prefs;
    prefs.begin("sequences", true);
    
    int count = prefs.getInt("count", 0);
    deviceSequences.clear();
    
    for (int i = 0; i < count; i++) {
        String key = "seq_" + String(i);
        String data = prefs.getString(key.c_str(), "");
        
        if (data.length() > 0) {
            // Parse: "deviceId|name|count1,count2,count3..."
            int firstPipe = data.indexOf('|');
            int secondPipe = data.indexOf('|', firstPipe + 1);
            
            if (firstPipe != -1 && secondPipe != -1) {
                PillSequence sequence;
                sequence.deviceId = data.substring(0, firstPipe);
                sequence.name = data.substring(firstPipe + 1, secondPipe);
                
                String countsStr = data.substring(secondPipe + 1);
                sequence.servoCounts.clear();
                
                // Parse counts
                int startIdx = 0;
                for (int j = 0; j <= countsStr.length(); j++) {
                    if (j == countsStr.length() || countsStr.charAt(j) == ',') {
                        if (j > startIdx) {
                            String numStr = countsStr.substring(startIdx, j);
                            sequence.servoCounts.push_back(numStr.toInt());
                        }
                        startIdx = j + 1;
                    }
                }
                
                deviceSequences[sequence.deviceId].push_back(sequence);
            }
        }
    }
    
    prefs.end();
    Displayer::getInstance().logMessage("[SEQ] Loaded " + String(count) + " sequences from storage");
}
