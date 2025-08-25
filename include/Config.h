// include/Config.h
#pragma once

#include <Arduino.h>

namespace Config {
    // WiFi Configuration
    constexpr const char* ssid = "PPC";
    constexpr const char* password = "2720vanlose2720";

    // Pin Configuration
    constexpr int LED_PIN = 2;
    constexpr int SERVO_PINS[] = {14, 15, 16, 17, 18, 19};
    constexpr int NUM_SERVOS = sizeof(SERVO_PINS) / sizeof(SERVO_PINS[0]);
    constexpr int PIEZO_PINS[] = {34};
    constexpr const char* PIEZO_NAMES[] = {"GREEN"};
    constexpr int NUM_PIEZOS = sizeof(PIEZO_PINS) / sizeof(PIEZO_PINS[0]);
    
    // Sensor Configuration
    constexpr int PIEZO_THRESHOLD = 50;

    // Servo Configuration
    constexpr int DEFAULT_ANGLE = 80;
    constexpr int DEFAULT_START_ANGLE = 0;
    constexpr int SERVO_DELAY_MS = 500;

    // Task Configuration
    constexpr int TASK_STACK_SIZE = 4096;
    constexpr int TASK_DELAY_MS = 10;

    // Network Configuration
    constexpr int WEB_SERVER_PORT = 80;
    constexpr int WEBSOCKET_PORT = 81;
};
