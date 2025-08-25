// src/main.cpp
#include <Arduino.h>
#include "Config.h"
#include "ServoController.h"
#include "PiezoController.h"
#include "SequenceManager.h"
#include "CommandHandler.h"
#include "Displayer.h"

// Global objects
ServoController servoController;
PiezoController piezoController;
SequenceManager sequenceManager(servoController);
CommandHandler commandHandler(servoController, piezoController, sequenceManager);

void setup() {
    Serial.begin(115200);
    
    // Initialize all components
    Displayer::getInstance().initialize();
    piezoController.initialize();
    servoController.initialize();
    servoController.setPiezoController(&piezoController);
    sequenceManager.initialize();
    commandHandler.initialize();
    
    // Set up the piezo log callback
    piezoController.setLogCallback([](const String& msg) {
        Displayer::getInstance().logMessage(msg);
    });
    
    // Start tasks
    piezoController.startTask();
    commandHandler.startTask();
}

void loop() {
    Displayer::getInstance().handleClients();
}