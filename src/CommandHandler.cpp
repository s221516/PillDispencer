// src/CommandHandler.cpp
#include "CommandHandler.h"

CommandHandler::CommandHandler(ServoController& servo, PiezoSensor& piezo, SequenceManager& sequenceManager)
    : servoController(servo), piezoController(piezo), sequenceManager(sequenceManager) {
}

void CommandHandler::initialize() {
    Displayer::getInstance().logMessage("Dispenser started");
    Displayer::getInstance().logMessage("Commands: reset | test | ANGLE <value> | STARTANGLE <value> | PILL <value> | MEASUREMENTS <value>");
    Displayer::getInstance().logMessage("Fast dispense: FAST <servo_number> - test fast dispensing");
    Displayer::getInstance().logMessage("Sequence commands: SEQUENCE <device> <name> (1,1,0,2,0,6) | EXECUTE <device> <name> | LIST <device> | DELETE <device> <name>");
}

void CommandHandler::startTask() {
    xTaskCreate(commandTaskWrapper, "Servo Control", Config::TASK_STACK_SIZE, this, 1, NULL);
}

void CommandHandler::commandTaskWrapper(void* parameter) {
    CommandHandler* handler = static_cast<CommandHandler*>(parameter);
    handler->commandTask();
}

void CommandHandler::commandTask() {
    while (true) {
        String command = "";
        bool hasCommand = false;
        
        // Check Serial first
        if (Serial.available() > 0) {
            command = Serial.readStringUntil('\n');
            hasCommand = true;
        }
        
        // Check WebSocket command buffer
        if (!hasCommand) {
            String webCommand = Displayer::getInstance().getCommandBuffer();
            if (webCommand.length() > 0) {
                command = webCommand;
                hasCommand = true;
                Displayer::getInstance().logMessage("[QUEUE] Processing: " + command + " (remaining: " + String(Displayer::getInstance().hasCommands() ? "yes" : "no") + ")");
                Displayer::getInstance().clearCommandBuffer();
            }
        }

        // Process command if we have one
        if (hasCommand && command.length() > 0) {
            command.trim();
            // Skip empty commands after trimming
            if (command.length() > 0) {
                Displayer::getInstance().logMessage("[CMD] About to process: " + command);
                processCommand(command);
                Displayer::getInstance().logMessage("[CMD] Finished processing: " + command);
            }
        }

        vTaskDelay(Config::TASK_DELAY_MS / portTICK_PERIOD_MS);
    }
}

void CommandHandler::processCommand(const String& command) {
    if (command.equalsIgnoreCase("reset")) {
        handleResetCommand();
    }
    else if (command.equalsIgnoreCase("test")) {
        handleTestCommand();
    }
    else if (command.startsWith("FAST")) {
        handleFastCommand(command);
    }
    else if (command.startsWith("ANGLE")) {
        handleAngleCommand(command);
    }
    else if (command.startsWith("STARTANGLE")) {
        handleStartAngleCommand(command);
    }
    else if (command.startsWith("PILL")) {
        handleIndividualPill(command);
    }
    else if (command.startsWith("MEASUREMENTS")) {
        handleMeasurementsCommand(command);
    }
    else if (command.startsWith("SEQUENCE")) {
        handleSequenceCommand(command);
    }
    else if (command.startsWith("EXECUTE")) {
        handleExecuteCommand(command);
    }
    else if (command.startsWith("LIST")) {
        handleListCommand(command);
    }
    else if (command.startsWith("DELETE")) {
        handleDeleteCommand(command);
    }
    else if (command.startsWith("RESETDATA")) {
        handleResetDataCommand(command);
    }
    else if (command.startsWith("THRESHOLD")) {
        handleThresholdCommand(command);
    }
    else {
        Displayer::getInstance().logMessage("[ERR] Unknown command: " + command);
    }
}

void CommandHandler::handleResetCommand() {
    servoController.resetAllServos();
    Displayer::getInstance().logMessage("[CMD] Servo reset.");
}

void CommandHandler::handleTestCommand() {
    servoController.resetCounter();
    Displayer::getInstance().logMessage("[CMD] Servo counter reset.");
}

void CommandHandler::handleAngleCommand(const String& command) {
    int value = command.substring(6).toInt();
    if (value >= 0 && value <= 180) {
        servoController.setAngle(value);
        Displayer::getInstance().logMessage("[CMD] Angle updated to " + String(value) + "°");
    } else {
        Displayer::getInstance().logMessage("[ERR] Invalid ANGLE value. Must be 0–180.");
    }
}

void CommandHandler::handleIndividualPill(const String& command) {
    int value = command.substring(5).toInt();
    
    if (value >= 1 && value <= 6) {
        int servoIndex = value - 1; // Convert to 0-based index
        Displayer::getInstance().logMessage("[CMD] Dispensing pill from servo " + String(value));
        
        if (servoController.Dispense(servoIndex)) {
            Displayer::getInstance().logMessage("[CMD] Pill successfully dispensed from servo " + String(value));
        } else {
            Displayer::getInstance().logMessage("[CMD] Failed to dispense pill from servo " + String(value) + " - check if bottle is empty");
        }
    }
}

void CommandHandler::handleMeasurementsCommand(const String& command) {
    int value = command.substring(12).toInt(); // Remove "MEASUREMENTS "
    if (value >= 1) {  // No upper limit
        piezoController.setPiezoMeasurements(value);
        Displayer::getInstance().logMessage("[CMD] Piezo measurements updated to " + String(value));
    } else {
        Displayer::getInstance().logMessage("[ERR] Invalid MEASUREMENTS value. Must be >= 1.");
    }
}

void CommandHandler::handleStartAngleCommand(const String& command) {
    int value = command.substring(11).toInt();
    if (value >= 0 && value <= 180) {
        servoController.setStartAngle(value);
        Displayer::getInstance().logMessage("[CMD] Start angle updated to " + String(value) + "°");
    } else {
        Displayer::getInstance().logMessage("[ERR] Invalid START value. Must be 0–180.");
    }
}

void CommandHandler::handleSequenceCommand(const String& command) {
    String deviceId, name;
    std::vector<int> counts;
    
    if (sequenceManager.parseSequenceCommand(command, deviceId, name, counts)) {
        if (sequenceManager.storeSequence(deviceId, name, counts)) {
            Displayer::getInstance().logMessage("[CMD] Sequence stored successfully");
        } else {
            Displayer::getInstance().logMessage("[ERR] Failed to store sequence");
        }
    } else {
        Displayer::getInstance().logMessage("[ERR] Invalid sequence format. Use: SEQUENCE <device> <name> (1,1,0,2,0,6)");
    }
}

void CommandHandler::handleExecuteCommand(const String& command) {
    // Expected format: "EXECUTE device123 morning"
    String params = command.substring(8); // Remove "EXECUTE "
    int spaceIndex = params.indexOf(' ');
    
    if (spaceIndex == -1) {
        Displayer::getInstance().logMessage("[ERR] Invalid execute format. Use: EXECUTE <device> <name>");
        return;
    }
    
    String deviceId = params.substring(0, spaceIndex);
    String name = params.substring(spaceIndex + 1);
    name.trim();
    
    if (sequenceManager.executeSequence(deviceId, name)) {
        Displayer::getInstance().logMessage("[CMD] Sequence executed successfully");
    } else {
        Displayer::getInstance().logMessage("[ERR] Failed to execute sequence");
    }
}

void CommandHandler::handleListCommand(const String& command) {
    // Expected format: "LIST device123"
    String deviceId = command.substring(5); // Remove "LIST "
    deviceId.trim();
    
    auto sequences = sequenceManager.getSequenceNames(deviceId);
    if (sequences.empty()) {
        Displayer::getInstance().logMessage("[INFO] No sequences found for device " + deviceId);
    } else {
        Displayer::getInstance().logMessage("[INFO] Sequences for device " + deviceId + ":");
        for (const auto& seqName : sequences) {
            Displayer::getInstance().logMessage("  - " + seqName);
        }
    }
}

void CommandHandler::handleDeleteCommand(const String& command) {
    // Expected format: "DELETE device123 sequenceName"
    String params = command.substring(7); // Remove "DELETE "
    int spaceIndex = params.indexOf(' ');
    
    if (spaceIndex == -1) {
        Displayer::getInstance().logMessage("[ERR] Invalid DELETE format. Use: DELETE <device> <sequence_name>");
        return;
    }
    
    String deviceId = params.substring(0, spaceIndex);
    String sequenceName = params.substring(spaceIndex + 1);
    deviceId.trim();
    sequenceName.trim();
    
    if (sequenceManager.deleteSequence(deviceId, sequenceName)) {
        Displayer::getInstance().logMessage("[CMD] Sequence '" + sequenceName + "' deleted successfully");
    } else {
        Displayer::getInstance().logMessage("[ERR] Sequence '" + sequenceName + "' not found for device " + deviceId);
    }
}

void CommandHandler::handleFastCommand(const String& command) {
    // Expected format: "FAST 1" (servo number 1-6)
    String params = command.substring(5); // Remove "FAST "
    params.trim();
    
    int servoNum = params.toInt();
    if (servoNum < 1 || servoNum > Config::NUM_SERVOS) {
        Displayer::getInstance().logMessage("[ERR] Invalid servo number. Use: FAST <1-" + String(Config::NUM_SERVOS) + ">");
        return;
    }
    
    int servoIndex = servoNum - 1; // Convert to 0-based index
    Displayer::getInstance().logMessage("[CMD] Testing fast dispense on servo " + String(servoNum));
    
    if (servoController.Dispense(servoIndex, 5)) {
        Displayer::getInstance().logMessage("[CMD] Fast dispense test successful");
    } else {
        Displayer::getInstance().logMessage("[CMD] Fast dispense test failed");
    }
}

void CommandHandler::handleResetDataCommand(const String& command) {
    // Extract parameter: "RESETDATA N" or "RESETDATA ALL"
    int spaceIndex = command.indexOf(' ');
    if (spaceIndex == -1) {
        Displayer::getInstance().logMessage("[ERR] Usage: RESETDATA <servo_number> or RESETDATA ALL");
        return;
    }
    
    String parameter = command.substring(spaceIndex + 1);
    parameter.trim();
    parameter.toUpperCase();
    
    if (parameter.equals("ALL")) {
        // Reset all servo data
        piezoController.resetAllData();
        Displayer::getInstance().logMessage("[CMD] All learning data has been reset for all dispensers");
    } else {
        // Try to parse servo number
        int servoNum = parameter.toInt();
        if (servoNum >= 1 && servoNum <= Config::NUM_SERVOS) {
            int servoIndex = servoNum - 1; // Convert to 0-based index
            piezoController.resetServoData(servoIndex);
            Displayer::getInstance().logMessage("[CMD] Learning data reset for dispenser " + String(servoNum));
        } else {
            Displayer::getInstance().logMessage("[ERR] Invalid servo number. Use 1-" + String(Config::NUM_SERVOS) + " or ALL");
        }
    }
}

void CommandHandler::handleThresholdCommand(const String& command) {
    // Command format: THRESHOLD GET or THRESHOLD SET AVERAGE <value> or THRESHOLD SET CHANNEL <value>
    int firstSpace = command.indexOf(' ');
    if (firstSpace == -1) {
        Displayer::getInstance().logMessage("[ERR] Invalid threshold command format");
        return;
    }
    
    String subCommand = command.substring(firstSpace + 1);
    
    if (subCommand.startsWith("GET")) {
        float avgThreshold = piezoController.getAverageThreshold();
        float chanThreshold = piezoController.getChannelThreshold();
        
        Displayer::getInstance().logMessage("[THRESH] Average: " + String(avgThreshold, 3) + 
                                          ", Channel: " + String(chanThreshold, 3));
    }
    else if (subCommand.startsWith("SET")) {
        // Parse "SET AVERAGE 0.85" or "SET CHANNEL 0.75"
        String setParams = subCommand.substring(4); // Remove "SET "
        int spacePos = setParams.indexOf(' ');
        
        if (spacePos == -1) {
            Displayer::getInstance().logMessage("[ERR] Invalid threshold SET format");
            return;
        }
        
        String thresholdType = setParams.substring(0, spacePos);
        String valueStr = setParams.substring(spacePos + 1);
        
        float value = valueStr.toFloat();
        
        if (thresholdType == "AVERAGE") {
            if (piezoController.setAverageThreshold(value)) {
                Displayer::getInstance().logMessage("[THRESH] Average threshold set to " + String(value, 3));
            } else {
                Displayer::getInstance().logMessage("[ERR] Invalid average threshold value (must be 0.0-1.0)");
            }
        }
        else if (thresholdType == "CHANNEL") {
            if (piezoController.setChannelThreshold(value)) {
                Displayer::getInstance().logMessage("[THRESH] Channel threshold set to " + String(value, 3));
            } else {
                Displayer::getInstance().logMessage("[ERR] Invalid channel threshold value (must be 0.0-1.0)");
            }
        }
        else {
            Displayer::getInstance().logMessage("[ERR] Unknown threshold type: " + thresholdType);
        }
    }
    else {
        Displayer::getInstance().logMessage("[ERR] Unknown threshold command: " + subCommand);
    }
}
