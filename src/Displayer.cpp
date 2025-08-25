// src/Displayer.cpp
#include "Displayer.h"

Displayer& Displayer::getInstance() {
    static Displayer instance;
    return instance;
}

void Displayer::initialize() {
    initSPIFFS();
    connectToWiFi();
    setupWebServer();
    setWebSocketEventHandler();
}

void Displayer::connectToWiFi() {
    WiFi.begin(Config::ssid, Config::password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());
}

void Displayer::setupWebServer() {
    Serial.println("Setting up web server...");
    
    // Serve the main app at root
    server.on("/", [this]() { 
        Serial.println("Root path requested");
        if (!handleFileRead("/index.html")) {
            Serial.println("Failed to serve index.html");
            server.send(404, "text/plain", "File not found");
        }
    });
    
    // Keep test page for debugging
    server.on("/test", [this]() { 
        Serial.println("Test path requested");
        server.send(200, "text/html", "<html><body><h1>ESP32 Web Server Test</h1><p>Server is working!</p></body></html>");
    });
    
    // Handle other static files
    server.onNotFound([this]() {
        Serial.println("File not found: " + server.uri());
        if (!handleFileRead(server.uri())) {
            server.send(404, "text/plain", "File not found: " + server.uri());
        }
    });
    
    server.begin();
    webSocket.begin();
    Serial.println("Web server started on port 80");
    Serial.println("WebSocket server started on port 81");
}

void Displayer::setWebSocketEventHandler() {
    webSocket.onEvent(onWebSocketEvent);
}

void Displayer::onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    if (type == WStype_TEXT) {
        String incoming = String((char*)payload);
        Serial.println("[WS] Received: " + incoming);
        
        // Immediately acknowledge receipt to sender
        getInstance().webSocket.sendTXT(num, "[ACK] Received: " + incoming);
        
        // Add command to queue (thread-safe)
        char* commandCopy = (char*)malloc(incoming.length() + 1);
        strcpy(commandCopy, incoming.c_str());
        
        BaseType_t result = xQueueSend(getInstance().commandQueue, &commandCopy, 0);
        if (result != pdTRUE) {
            Serial.println("[ERROR] Command queue full! Dropping command: " + incoming);
            free(commandCopy);  // Free memory if queue send failed
            
            // Send error message to client
            getInstance().webSocket.sendTXT(num, "[ERROR] Command queue full - command dropped");
        } else {
            UBaseType_t queueSize = uxQueueMessagesWaiting(getInstance().commandQueue);
            Serial.println("[QUEUE] Command added. Queue size: " + String(queueSize));
        }
    } else if (type == WStype_CONNECTED) {
        getInstance().handleDeviceConnection(num);
    } else if (type == WStype_DISCONNECTED) {
        getInstance().handleDeviceDisconnection(num);
    }
}

void Displayer::broadcast(const String& message) {
    String msg = message;  // Create non-const copy for WebSocket library
    webSocket.broadcastTXT(msg);
    updateConnectedDevicesActivity();
}

String Displayer::getCommandBuffer() {
    char* command = nullptr;
    
    // Try to receive command from queue with no wait
    if (xQueueReceive(commandQueue, &command, 0) == pdTRUE && command != nullptr) {
        String result = String(command);
        UBaseType_t queueSize = uxQueueMessagesWaiting(commandQueue);
        Serial.println("[QUEUE] Command retrieved: " + result + ". Remaining: " + String(queueSize));
        
        free(command);  // Free the malloc'd memory
        return result;
    }
    
    return "";  // Return empty string if no commands available
}

void Displayer::clearCommandBuffer() {
    // This method is no longer needed since getCommandBuffer() 
    // already removes the command from the queue
    // Do nothing - queue automatically manages itself
}

bool Displayer::hasCommands() {
    return uxQueueMessagesWaiting(commandQueue) > 0;
}

void Displayer::initSPIFFS() {
    if (!SPIFFS.begin(true)) {
        Serial.println("An error occurred while mounting SPIFFS");
        return;
    }
    Serial.println("SPIFFS mounted successfully");
}

String Displayer::getContentType(String filename) {
    if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".png")) return "image/png";
    else if (filename.endsWith(".gif")) return "image/gif";
    else if (filename.endsWith(".jpg")) return "image/jpeg";
    else if (filename.endsWith(".ico")) return "image/x-icon";
    else if (filename.endsWith(".xml")) return "text/xml";
    else if (filename.endsWith(".pdf")) return "application/x-pdf";
    else if (filename.endsWith(".zip")) return "application/x-zip";
    else if (filename.endsWith(".gz")) return "application/x-gzip";
    return "text/plain";
}

bool Displayer::handleFileRead(String path) {
    Serial.println("handleFileRead: " + path);
    
    if (path.endsWith("/")) {
        path += "index.html";
    }
    
    String contentType = getContentType(path);
    String pathWithGz = path + ".gz";
    
    if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
        if (SPIFFS.exists(pathWithGz)) {
            path += ".gz";
        }
        
        File file = SPIFFS.open(path, "r");
        if (!file) {
            Serial.println("Failed to open file for reading");
            return false;
        }
        
        server.streamFile(file, contentType);
        file.close();
        Serial.println("File sent: " + path);
        return true;
    }
    
    Serial.println("File not found: " + path);
    return false;
}

void Displayer::sendConnectedDevices() {
    String deviceList = "[DEVICES] Connected devices:\n";
    for (const auto& pair : connectedDevices) {
        deviceList += "  - Client " + String(pair.first) + " (last seen: " + 
                     String((millis() - pair.second.lastActivity) / 1000) + "s ago)\n";
    }
    broadcast(deviceList);
}

int Displayer::getConnectedDeviceCount() {
    return connectedDevices.size();
}

void Displayer::handleDeviceConnection(uint8_t clientId) {
    ConnectedDevice device;
    device.connectedTime = millis();
    device.lastActivity = millis();
    connectedDevices[clientId] = device;
    
    String message = "[CONNECT] Client " + String(clientId) + " connected";
    Serial.println(message);
    broadcast(message);
}

void Displayer::handleDeviceDisconnection(uint8_t clientId) {
    connectedDevices.erase(clientId);
    
    String message = "[DISCONNECT] Client " + String(clientId) + " disconnected";
    Serial.println(message);
    broadcast(message);
}

void Displayer::updateDeviceActivity(uint8_t clientId) {
    auto it = connectedDevices.find(clientId);
    if (it != connectedDevices.end()) {
        it->second.lastActivity = millis();
    }
}

void Displayer::updateConnectedDevicesActivity() {
    for (auto& pair : connectedDevices) {
        pair.second.lastActivity = millis();
    }
}

void Displayer::logMessage(const String& msg) {
    Serial.println(msg);
    broadcast(msg);
}

void Displayer::handleClients() {
    server.handleClient();  // Process HTTP requests
    webSocket.loop();       // Process WebSocket connections
}
