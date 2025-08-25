// src/CommandHandler.cpp
#include "CommandHandler.h"

CommandHandler::CommandHandler(ServoController& servo, PiezoController& piezo, SequenceManager& sequenceManager)
    : servoController(servo), piezoController(piezo), sequenceManager(sequenceManager) {
}

void CommandHandler::initialize() {
    Displayer::getInstance().logMessage("Dispenser started");
    Displayer::getInstance().logMessage("Commands: reset | [ENTER] toggle | ANGLE <value> | START <value> | test");
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
        
        // Check Serial first
        if (Serial.available() > 0) {
            command = Serial.readStringUntil('\n');
        }
        
        // Check WebSocket command buffer
        String webCommand = Displayer::getInstance().getCommandBuffer();
        if (webCommand.length() > 0) {
            command = webCommand;
            Displayer::getInstance().logMessage("[QUEUE] Processing: " + command + " (remaining: " + String(Displayer::getInstance().hasCommands() ? "yes" : "no") + ")");
            Displayer::getInstance().clearCommandBuffer();
        }

        // Process command if we have one
        if (command.length() > 0) {
            command.trim();
            Displayer::getInstance().logMessage("[CMD] About to process: " + command);
            processCommand(command);
            Displayer::getInstance().logMessage("[CMD] Finished processing: " + command);
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
    else if (command.length() == 0) {
        handleToggleCommand();
    }
    else if (command.startsWith("ANGLE")) {
        handleAngleCommand(command);
    }
    else if (command.startsWith("START")) {
        handleStartCommand(command);
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

void CommandHandler::handleToggleCommand() {
    servoController.toggle();
    if (servoController.isAtStart()) {
        Displayer::getInstance().logMessage("[RUN] Start angle: " + 
            String(servoController.getStartAngle()) + "° | Counter: " + 
            String(servoController.getCounter()));
    } else {
        Displayer::getInstance().logMessage("[RUN] Angle: " + 
            String(servoController.getAngle()) + "° | Counter: " + 
            String(servoController.getCounter()));
    }
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

void CommandHandler::handleStartCommand(const String& command) {
    int value = command.substring(6).toInt();
    
    // Check if this is for individual servo dispensing (START 1-6)
    if (value >= 1 && value <= 6) {
        int servoIndex = value - 1; // Convert to 0-based index
        Displayer::getInstance().logMessage("[CMD] Dispensing pill from servo " + String(value));
        
        if (servoController.fastDispenseWithFeedback(servoIndex)) {
            Displayer::getInstance().logMessage("[CMD] Pill successfully dispensed from servo " + String(value));
        } else {
            Displayer::getInstance().logMessage("[CMD] Failed to dispense pill from servo " + String(value) + " - check if bottle is empty");
        }
    }
    // Check if this is for setting start angle (START 0-180)
    else if (value >= 0 && value <= 180) {
        servoController.setStartAngle(value);
        Displayer::getInstance().logMessage("[CMD] Start angle updated to " + String(value) + "°");
    } else {
        Displayer::getInstance().logMessage("[ERR] Invalid START value. Use 1-6 for servo dispensing or 0-180 for start angle.");
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
    
    if (servoController.fastDispenseWithFeedback(servoIndex, 5)) {
        Displayer::getInstance().logMessage("[CMD] Fast dispense test successful");
    } else {
        Displayer::getInstance().logMessage("[CMD] Fast dispense test failed");
    }
}
