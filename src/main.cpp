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
PiezoSensor piezoSensor;
SequenceManager sequenceManager(servoController);
CommandHandler commandHandler(servoController, piezoSensor, sequenceManager);

void setup() {
    Serial.begin(115200);
    
    // Initialize all components
    Displayer::getInstance().initialize();
    piezoSensor.initialize();  // Initialize piezo sensor
    servoController.initialize();
    servoController.setPiezoSensor(&piezoSensor);
    sequenceManager.initialize();
    commandHandler.initialize();
    
    // Set up the piezo log callback
    piezoSensor.setLogCallback([](const String& msg) {
        Displayer::getInstance().logMessage(msg);
    });
    
    // Start tasks
    commandHandler.startTask();
}

void loop() {
    Displayer::getInstance().handleClients();
}