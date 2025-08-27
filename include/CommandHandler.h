// include/CommandHandler.h
#pragma once
#include <Arduino.h>
#include "ServoController.h"
#include "PiezoController.h"
#include "SequenceManager.h"
#include "Displayer.h"
#include "Config.h"

// Forward declaration
class ServoController;

class CommandHandler {
public:
    CommandHandler(ServoController& servo, PiezoSensor& piezo, SequenceManager& sequenceManager);
    void initialize();
    void startTask();

private:
    ServoController& servoController;
    PiezoSensor& piezoController;
    SequenceManager& sequenceManager;
    
    static void commandTaskWrapper(void* parameter);
    void commandTask();
    void processCommand(const String& command);
    void handleResetCommand();
    void handleTestCommand();
    void handleFastCommand(const String& command);
    void handleStartAngleCommand(const String& command);
    void handleAngleCommand(const String& command);
    void handleIndividualPill(const String& command);
    void handleMeasurementsCommand(const String& command);
    void handleSequenceCommand(const String& command);
    void handleExecuteCommand(const String& command);
    void handleListCommand(const String& command);
    void handleDeleteCommand(const String& command);
    void handleResetDataCommand(const String& command);
    void handleThresholdCommand(const String& command);
    void handlePiezoCommand(const String& command);
};