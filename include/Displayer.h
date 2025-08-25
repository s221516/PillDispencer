// include/Displayer.h
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <map>
#include <queue>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "Config.h"

struct ConnectedDevice {
    uint8_t clientId;
    String userAgent;
    String ipAddress;
    unsigned long connectedTime;
    unsigned long lastActivity;
};

class Displayer {
public:
    static Displayer& getInstance();
    void initialize();
    void handleClients();
    void logMessage(const String& msg);
    void setWebSocketEventHandler();
    String getCommandBuffer();
    void clearCommandBuffer();
    bool hasCommands();  // Check if there are queued commands
    void sendConnectedDevices();
    int getConnectedDeviceCount();

private:
    Displayer() : server(Config::WEB_SERVER_PORT), webSocket(Config::WEBSOCKET_PORT) {
        // Create command queue that can hold up to 50 commands (increased from 10)
        commandQueue = xQueueCreate(50, sizeof(char*));
    }
    WebServer server;
    WebSocketsServer webSocket;
    String commandBuffer;  // Keep for backward compatibility
    QueueHandle_t commandQueue;  // FreeRTOS queue for commands
    std::map<uint8_t, ConnectedDevice> connectedDevices;
    
    static void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
    void connectToWiFi();
    void setupWebServer();
    static const char* getHtmlPage();
    void handleDeviceConnection(uint8_t clientId);
    void handleDeviceDisconnection(uint8_t clientId);
    void updateDeviceActivity(uint8_t clientId);
};