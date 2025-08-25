// include/SequenceManager.h
#pragma once
#include <Arduino.h>
#include <vector>
#include <map>
#include "Config.h"

// Forward declaration
class ServoController;

struct PillSequence {
    String deviceId;
    std::vector<int> servoCounts;  // How many times each servo runs
    String name;
    unsigned long timestamp;
};

class SequenceManager {
public:
    SequenceManager(ServoController& servoController);
    void initialize();
    
    // Sequence management
    bool storeSequence(const String& deviceId, const String& name, const std::vector<int>& counts);
    bool executeSequence(const String& deviceId, const String& name);
    bool deleteSequence(const String& deviceId, const String& name);
    std::vector<String> getSequenceNames(const String& deviceId);
    
    // Command parsing
    bool parseSequenceCommand(const String& command, String& deviceId, String& name, std::vector<int>& counts);
    
    // Persistence
    void saveToStorage();
    void loadFromStorage();

private:
    ServoController& servoController;
    std::map<String, std::vector<PillSequence>> deviceSequences;  // deviceId -> sequences
    
    String generateKey(const String& deviceId, const String& name);
    void executeServoSequence(const std::vector<int>& counts);
};
