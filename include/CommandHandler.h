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
    CommandHandler(ServoController& servo, PiezoController& piezo, SequenceManager& sequenceManager);
    void initialize();
    void startTask();

private:
    ServoController& servoController;
    PiezoController& piezoController;
    SequenceManager& sequenceManager;
    
    static void commandTaskWrapper(void* parameter);
    void commandTask();
    void processCommand(const String& command);
    void handleResetCommand();
    void handleTestCommand();
    void handleFastCommand(const String& command);
    void handleToggleCommand();
    void handleAngleCommand(const String& command);
    void handleStartCommand(const String& command);
    void handleSequenceCommand(const String& command);
    void handleExecuteCommand(const String& command);
    void handleListCommand(const String& command);
    void handleDeleteCommand(const String& command);
    void handlePiezoCommand(const String& command);
};