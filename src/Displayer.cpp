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
        // Check rate limiting first
        if (getInstance().isClientThrottled(num)) {
            Serial.println("[THROTTLE] Blocking command from throttled client " + String(num));
            
            // Send throttle reminder every 5 seconds so user knows they're still blocked
            ConnectedDevice& device = getInstance().connectedDevices[num];
            unsigned long currentTime = millis();
            static unsigned long lastThrottleReminder = 0;
            
            if (currentTime - lastThrottleReminder > 5000) {  // Every 5 seconds
                unsigned long timeLeft = (device.throttleEndTime - currentTime) / 1000;
                String reminderMsg = "[THROTTLE] ⏳ Still cooling down! " + String(timeLeft) + " seconds remaining...";
                getInstance().webSocket.sendTXT(num, reminderMsg.c_str());
                lastThrottleReminder = currentTime;
            }
            return;  // Drop commands from throttled clients
        }
        
        // Properly handle payload with known length
        String incoming = "";
        if (length > 0 && payload != nullptr) {
            // Create a null-terminated string from payload
            char* buffer = (char*)malloc(length + 1);
            memcpy(buffer, payload, length);
            buffer[length] = '\0';
            incoming = String(buffer);
            free(buffer);
        }
        
        // Skip empty commands
        incoming.trim();
        if (incoming.length() == 0) {
            Serial.println("[WS] Ignoring empty command");
            return;
        }
        
        // Update rate limiting for this client
        getInstance().updateClientRateLimit(num);
        
        // Check if client got throttled by this command
        if (getInstance().isClientThrottled(num)) {
            Serial.println("[THROTTLE] Client " + String(num) + " throttled after command: " + incoming);
            return;  // Don't process this command
        }
        
        Serial.println("[WS] Received: " + incoming);
        
        // Check queue size before sending ACK
        UBaseType_t currentQueueSize = uxQueueMessagesWaiting(getInstance().commandQueue);
        
        // Stricter throttling for PILL commands to prevent crashes
        if (incoming.startsWith("PILL ")) {
            // Only send ACK for first command, throttle the rest more aggressively
            if (currentQueueSize == 0) {
                String ackMsg = "[ACK] Received: " + incoming;
                getInstance().webSocket.sendTXT(num, ackMsg.c_str());
            } else {
                // For all subsequent PILL commands, just log to serial
                Serial.println("[ACK] Received (throttled): " + incoming);
            }
        } else {
            // Send ACK for non-PILL commands
            String ackMsg = "[ACK] Received: " + incoming;
            getInstance().webSocket.sendTXT(num, ackMsg.c_str());
        }
        
        // Add command to queue (thread-safe) with bounds checking
        if (incoming.length() > 100) {  // Sanity check for reasonable command length
            Serial.println("[ERROR] Command too long, ignoring: " + String(incoming.length()) + " chars");
            return;
        }
        
        char* commandCopy = (char*)malloc(incoming.length() + 1);
        if (commandCopy == NULL) {
            Serial.println("[ERROR] Failed to allocate memory for command");
            return;
        }
        strcpy(commandCopy, incoming.c_str());
        
        BaseType_t result = xQueueSend(getInstance().commandQueue, &commandCopy, 0);
        if (result != pdTRUE) {
            Serial.println("[ERROR] Command queue full! Dropping command: " + incoming);
            free(commandCopy);  // Free memory if queue send failed
            
            // Send error message safely
            String errorMsg = "[ERROR] QUEUE FULL! Command dropped.";
            getInstance().webSocket.sendTXT(num, errorMsg.c_str());
            getInstance().logMessage("[QUEUE] Queue full! Dropped command (Queue limit: 10)");
        } else {
            UBaseType_t queueSize = uxQueueMessagesWaiting(getInstance().commandQueue);
            // Only send queue status for every 3rd queued command to reduce WebSocket spam
            static int queueMessageCounter = 0;
            queueMessageCounter++;
            
            if (queueMessageCounter % 3 == 0 || queueSize <= 1) {
                String queueMsg = "[QUEUE] Processing: " + incoming + " (remaining: " + String(queueSize - 1) + ")";
                getInstance().logMessage(queueMsg);
            }
            
            // Always log to serial for debugging
            Serial.println("[QUEUE] Processing: " + incoming + " (remaining: " + String(queueSize - 1) + ")");
        }
    } else if (type == WStype_CONNECTED) {
        getInstance().handleDeviceConnection(num);
    } else if (type == WStype_DISCONNECTED) {
        getInstance().handleDeviceDisconnection(num);
    }
}

void Displayer::broadcast(const String& message) {
    // Validate UTF-8 encoding before sending
    bool isValidUTF8 = true;
    for (int i = 0; i < message.length(); i++) {
        char c = message.charAt(i);
        // Check for control characters that could break UTF-8
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
            isValidUTF8 = false;
            Serial.println("[ERROR] Invalid character found at position " + String(i) + ": " + String((int)c));
            break;
        }
    }
    
    if (!isValidUTF8) {
        Serial.println("[ERROR] Message contains invalid UTF-8, not broadcasting");
        return;
    }
    
    // Add bounds checking and safety for WebSocket broadcast
    // Allow larger messages for graph data, smaller for other messages
    int maxLength = message.startsWith("[GRAPH]") ? 8000 : 500;
    
    if (message.length() > maxLength) {
        Serial.println("[WARN] Message too long for broadcast, truncating");
        String truncated = message.substring(0, maxLength) + "...";
        webSocket.broadcastTXT(truncated.c_str());
    } else {
        webSocket.broadcastTXT(message.c_str());
    }
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
    device.commandCount = 0;         // Initialize rate limiting
    device.windowStartTime = millis(); // Start rate limit window
    device.isThrottled = false;      // Not throttled initially
    device.throttleEndTime = 0;      // No throttle end time
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
    // Check if this is graph data - send to web but not to serial to keep serial clean
    if (msg.startsWith("[GRAPH]")) {
        broadcast(msg);  // Send to web interface for graphs
        // Don't print the full graph data to serial to avoid spam
    } else {
        Serial.println(msg);  // Print other messages to serial
        broadcast(msg);       // Send to web interface
    }
}

void Displayer::handleClients() {
    server.handleClient();  // Process HTTP requests
    webSocket.loop();       // Process WebSocket connections
}

bool Displayer::isClientThrottled(uint8_t clientId) {
    if (connectedDevices.find(clientId) == connectedDevices.end()) {
        return false;  // Client not found, not throttled
    }
    
    ConnectedDevice& device = connectedDevices[clientId];
    unsigned long currentTime = millis();
    
    // Check if throttle period has ended
    if (device.isThrottled && currentTime >= device.throttleEndTime) {
        device.isThrottled = false;
        device.commandCount = 0;
        device.windowStartTime = currentTime;
        Serial.println("[THROTTLE] Client " + String(clientId) + " throttle period ended");
        
        // Send "unblocked" notification to client
        String unblockedMsg = "[THROTTLE] ✅ Cooldown finished! You can now use the system again.";
        getInstance().webSocket.sendTXT(clientId, unblockedMsg.c_str());
    }
    
    return device.isThrottled;
}

void Displayer::updateClientRateLimit(uint8_t clientId) {
    // Rate limit: Max 10 commands per 5 seconds, 30 second cooldown if exceeded
    const unsigned long RATE_WINDOW = 5000;      // 5 seconds
    const unsigned long MAX_COMMANDS = 10;       // Max commands per window
    const unsigned long THROTTLE_DURATION = 30000; // 30 second cooldown
    
    if (connectedDevices.find(clientId) == connectedDevices.end()) {
        return;  // Client not found
    }
    
    ConnectedDevice& device = connectedDevices[clientId];
    unsigned long currentTime = millis();
    
    // If already throttled, don't update counters
    if (device.isThrottled) {
        return;
    }
    
    // Reset window if enough time has passed
    if (currentTime - device.windowStartTime > RATE_WINDOW) {
        device.commandCount = 0;
        device.windowStartTime = currentTime;
    }
    
    // Increment command count
    device.commandCount++;
    
    // Warn user when approaching rate limit
    if (device.commandCount == MAX_COMMANDS - 2) {  // 2 commands before limit
        String warningMsg = "[THROTTLE] ⚠️ Warning: Slow down! Only " + String(MAX_COMMANDS - device.commandCount) + " commands left before cooldown.";
        getInstance().webSocket.sendTXT(clientId, warningMsg.c_str());
    }
    
    // Check if rate limit exceeded
    if (device.commandCount > MAX_COMMANDS) {
        device.isThrottled = true;
        device.throttleEndTime = currentTime + THROTTLE_DURATION;
        
        Serial.println("[THROTTLE] Client " + String(clientId) + " rate limited for " + String(THROTTLE_DURATION/1000) + " seconds");
        
        // Send comprehensive throttle message to client
        String throttleMsg = "[THROTTLE] 🚫 RATE LIMIT EXCEEDED! 🚫\n";
        throttleMsg += "You sent too many commands too quickly.\n";
        throttleMsg += "Commands are blocked for " + String(THROTTLE_DURATION/1000) + " seconds.\n";
        throttleMsg += "Please wait before trying again...";
        webSocket.sendTXT(clientId, throttleMsg.c_str());
        
        // Also send a countdown notification
        String countdownMsg = "[THROTTLE] ⏰ Cooldown: " + String(THROTTLE_DURATION/1000) + " seconds remaining";
        webSocket.sendTXT(clientId, countdownMsg.c_str());
    }
}
