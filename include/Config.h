// include/Config.h
#pragma once

#include <Arduino.h>

namespace Config {
    // WiFi Configuration
    constexpr const char* ssid = "PPC";
    constexpr const char* password = "2720vanlose2720";

    // Pin Configuration
    constexpr int LED_PIN = 2;
    constexpr int SERVO_PINS[] = {26, 25};  // Pin 26 confirmed working, pin 25 for expansion
    constexpr int NUM_SERVOS = sizeof(SERVO_PINS) / sizeof(SERVO_PINS[0]);
    constexpr int PIEZO_PINS[] = {32,33};
    constexpr const char* PIEZO_NAMES[] = {"GREEN", "BLUE", "RED", "ORANGE", "PURPLE", "PINK"};
    constexpr int NUM_PIEZOS = sizeof(PIEZO_PINS) / sizeof(PIEZO_PINS[0]);
    
    // Sensor Configuration
    constexpr int PIEZO_THRESHOLD = 50;

    // Servo Configuration
    constexpr int RESET_ANGLE = 180;
    constexpr int DEFAULT_ANGLE = 70;        // Updated dispense angle
    constexpr int DEFAULT_START_ANGLE = 0;  // Updated start angle
    constexpr int SERVO_DELAY_MS = 500;

    //Piezo configuration
    constexpr int PIEZO_MEASUREMENTS = 10;
    constexpr int TASK_TIMEOUT_MS = 1000;

    // Task Configuration
    constexpr int TASK_STACK_SIZE = 8192;  // Increased for stability
    constexpr int TASK_DELAY_MS = 10;

    // Network Configuration
    constexpr int WEB_SERVER_PORT = 80;
    constexpr int WEBSOCKET_PORT = 81;
};
