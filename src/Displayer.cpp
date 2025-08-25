// src/Displayer.cpp
#include "Displayer.h"

Displayer& Displayer::getInstance() {
    static Displayer instance;
    return instance;
}

void Displayer::initialize() {
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
    server.on("/", [this]() { 
        server.send_P(200, "text/html", getHtmlPage()); 
    });
    server.begin();
    webSocket.begin();
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
        
        if (xQueueSend(getInstance().commandQueue, &commandCopy, 0) != pdTRUE) {
            // Queue is full, free the memory and log error
            free(commandCopy);
            Serial.println("[WS] Command queue full, dropping command: " + incoming);
            // Also send error to web interface
            getInstance().webSocket.broadcastTXT("[ERROR] Command queue full - command dropped!");
        } else {
            // Log successful queue addition
            Serial.println("[WS] Command queued successfully. Queue size: " + String(uxQueueMessagesWaiting(getInstance().commandQueue)));
        }
    }
}

void Displayer::handleClients() {
    webSocket.loop();
    server.handleClient();
}

void Displayer::logMessage(const String& msg) {
    Serial.println(msg);
    webSocket.broadcastTXT(msg.c_str());
}

String Displayer::getCommandBuffer() {
    char* command;
    if (xQueueReceive(commandQueue, &command, 0) == pdTRUE) {
        String result = String(command);
        free(command);  // Free the allocated memory
        return result;
    }
    return "";  // No commands in queue
}

void Displayer::clearCommandBuffer() {
    // This method is no longer needed since getCommandBuffer() 
    // already removes the command from the queue
    // Do nothing - queue automatically manages itself
}

bool Displayer::hasCommands() {
    return uxQueueMessagesWaiting(commandQueue) > 0;
}

const char* Displayer::getHtmlPage() {
    static const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>ESP32 Terminal</title>
  <style>
    body { 
      font-family: 'Courier New', monospace; 
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); 
      color: #ffffff; 
      margin: 0; 
      padding: 20px;
    }
    h2 { color: #ffffff; text-align: center; margin-bottom: 20px; }
    #log { 
      width: 100%; 
      height: 200px; 
      overflow-y: auto; 
      border: 2px solid #4fc3f7; 
      padding: 10px; 
      background: rgba(0, 0, 0, 0.7);
      color: #00ff41;
      border-radius: 8px;
      font-family: 'Courier New', monospace;
    }
    input { 
      width: 100%; 
      padding: 12px; 
      background: rgba(255, 255, 255, 0.9); 
      color: #333; 
      border: 2px solid #4fc3f7; 
      border-radius: 6px;
      font-size: 16px;
      margin-top: 10px;
    }
    input:focus { outline: none; border-color: #29b6f6; box-shadow: 0 0 10px rgba(79, 195, 247, 0.5); }
    .dispensers { 
      margin: 20px 0; 
      padding: 15px; 
      border: 2px solid #ff9800; 
      background: rgba(255, 152, 0, 0.1);
      border-radius: 8px;
    }
    .dispensers h3 { color: #ffffff; margin-top: 0; }
    #dispensers-container {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 10px;
      margin-bottom: 10px;
    }
    .dispenser-btn { 
      padding: 15px; 
      background: linear-gradient(45deg, #ff9800, #f57c00); 
      color: white; 
      border: none; 
      cursor: pointer; 
      border-radius: 8px;
      font-weight: bold;
      transition: all 0.3s;
      box-shadow: 0 2px 4px rgba(0,0,0,0.2);
      text-align: center;
      min-height: 60px;
    }
    .dispenser-btn:hover { 
      background: linear-gradient(45deg, #f57c00, #ef6c00); 
      transform: translateY(-2px);
      box-shadow: 0 4px 8px rgba(0,0,0,0.3);
    }
    .dispenser-btn small { font-size: 10px; opacity: 0.8; }
    .sequences { 
      margin: 20px 0; 
      padding: 15px; 
      border: 2px solid #4fc3f7; 
      background: rgba(255, 255, 255, 0.1);
      border-radius: 8px;
    }
    .sequences h3 { color: #ffffff; margin-top: 0; }
    .sequence-btn { 
      margin: 5px; 
      padding: 10px 16px; 
      background: linear-gradient(45deg, #4fc3f7, #29b6f6); 
      color: white; 
      border: none; 
      cursor: pointer; 
      border-radius: 6px;
      font-weight: bold;
      transition: all 0.3s;
      box-shadow: 0 2px 4px rgba(0,0,0,0.2);
    }
    .sequence-btn:hover { 
      background: linear-gradient(45deg, #29b6f6, #0288d1); 
      transform: translateY(-2px);
      box-shadow: 0 4px 8px rgba(0,0,0,0.3);
    }
    .delete-btn { 
      background: linear-gradient(45deg, #f44336, #d32f2f); 
      color: white; 
      margin-left: 5px; 
      padding: 8px 12px; 
      font-size: 14px;
    }
    .delete-btn:hover { 
      background: linear-gradient(45deg, #d32f2f, #b71c1c); 
    }
    .refresh-btn {
      background: linear-gradient(45deg, #66bb6a, #4caf50);
      color: white;
      border: none;
      padding: 8px 16px;
      border-radius: 6px;
      cursor: pointer;
      margin-top: 10px;
      transition: all 0.3s;
    }
    .refresh-btn:hover {
      background: linear-gradient(45deg, #4caf50, #388e3c);
      transform: translateY(-1px);
    }
    .main-content {
      display: flex;
      gap: 20px;
      margin: 20px 0;
    }
    .terminal-section {
      flex: 2;
    }
    .dispensing-log {
      flex: 1;
      padding: 15px;
      border: 2px solid #9c27b0;
      background: rgba(156, 39, 176, 0.1);
      border-radius: 8px;
      max-height: 400px;
    }
    .dispensing-log h3 { color: #ffffff; margin-top: 0; }
    #dispensing-log-container {
      height: 300px;
      overflow-y: auto;
      background: rgba(0, 0, 0, 0.3);
      padding: 10px;
      border-radius: 6px;
      font-family: 'Courier New', monospace;
      font-size: 12px;
      color: #e1bee7;
    }
    .log-entry {
      margin-bottom: 8px;
      padding: 5px;
      border-left: 3px solid #9c27b0;
      background: rgba(156, 39, 176, 0.1);
    }
    .log-time { color: #ba68c8; font-weight: bold; }
    .log-action { color: #ffffff; }
    .log-dispenser { color: #ffab40; }
    .clear-log-btn {
      background: linear-gradient(45deg, #9c27b0, #7b1fa2);
      color: white;
      border: none;
      padding: 6px 12px;
      border-radius: 4px;
      cursor: pointer;
      margin-top: 10px;
      font-size: 12px;
      transition: all 0.3s;
    }
    .clear-log-btn:hover {
      background: linear-gradient(45deg, #7b1fa2, #6a1b9a);
    }
    .sequence-builder {
      margin: 20px 0;
      padding: 15px;
      border: 2px solid #4caf50;
      background: rgba(76, 175, 80, 0.1);
      border-radius: 8px;
    }
    .sequence-builder h3 { color: #ffffff; margin-top: 0; }
    .builder-container {
      display: flex;
      flex-direction: column;
      gap: 15px;
    }
    .sequence-name-input {
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .sequence-name-input label {
      color: #ffffff;
      font-weight: bold;
      min-width: 120px;
    }
    .sequence-name-input input {
      flex: 1;
      padding: 8px 12px;
      border: 2px solid #4caf50;
      border-radius: 6px;
      background: rgba(255, 255, 255, 0.9);
      color: #333;
    }
    .dispenser-counts {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 10px;
    }
    .count-item {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 10px;
      background: rgba(255, 255, 255, 0.1);
      border-radius: 6px;
      border: 1px solid rgba(76, 175, 80, 0.3);
    }
    .count-item label {
      color: #ffffff;
      font-weight: bold;
      flex: 1;
    }
    .count-controls {
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .count-btn {
      width: 30px;
      height: 30px;
      border: none;
      border-radius: 50%;
      background: linear-gradient(45deg, #4caf50, #66bb6a);
      color: white;
      font-weight: bold;
      font-size: 16px;
      cursor: pointer;
      transition: all 0.3s;
    }
    .count-btn:hover {
      background: linear-gradient(45deg, #388e3c, #4caf50);
      transform: scale(1.1);
    }
    .count-display {
      min-width: 20px;
      text-align: center;
      color: #ffffff;
      font-weight: bold;
      font-size: 18px;
    }
    .builder-actions {
      display: flex;
      gap: 10px;
      justify-content: center;
    }
    .create-sequence-btn {
      background: linear-gradient(45deg, #4caf50, #66bb6a);
      color: white;
      border: none;
      padding: 12px 24px;
      border-radius: 8px;
      font-weight: bold;
      cursor: pointer;
      transition: all 0.3s;
      box-shadow: 0 2px 4px rgba(0,0,0,0.2);
    }
    .create-sequence-btn:hover {
      background: linear-gradient(45deg, #388e3c, #4caf50);
      transform: translateY(-2px);
      box-shadow: 0 4px 8px rgba(0,0,0,0.3);
    }
    .reset-builder-btn {
      background: linear-gradient(45deg, #ff9800, #ffb74d);
      color: white;
      border: none;
      padding: 12px 24px;
      border-radius: 8px;
      font-weight: bold;
      cursor: pointer;
      transition: all 0.3s;
      box-shadow: 0 2px 4px rgba(0,0,0,0.2);
    }
    .reset-builder-btn:hover {
      background: linear-gradient(45deg, #f57c00, #ff9800);
      transform: translateY(-2px);
      box-shadow: 0 4px 8px rgba(0,0,0,0.3);
    }
    .sequence-preview {
      padding: 10px;
      background: rgba(0, 0, 0, 0.3);
      border-radius: 6px;
      font-family: 'Courier New', monospace;
      color: #4caf50;
      text-align: center;
    }
  </style>
</head>
<body>
  <h2>ESP32 Web Terminal</h2>
  <div class="dispensers">
    <h3>Individual Dispensers:</h3>
    <div id="dispensers-container">
      <button class="dispenser-btn" data-dispenser="0">
        <span id="dispenser-name-0">Dispenser 1</span><br>
        <small>Double-click to rename</small>
      </button>
      <button class="dispenser-btn" data-dispenser="1">
        <span id="dispenser-name-1">Dispenser 2</span><br>
        <small>Double-click to rename</small>
      </button>
      <button class="dispenser-btn" data-dispenser="2">
        <span id="dispenser-name-2">Dispenser 3</span><br>
        <small>Double-click to rename</small>
      </button>
      <button class="dispenser-btn" data-dispenser="3">
        <span id="dispenser-name-3">Dispenser 4</span><br>
        <small>Double-click to rename</small>
      </button>
      <button class="dispenser-btn" data-dispenser="4">
        <span id="dispenser-name-4">Dispenser 5</span><br>
        <small>Double-click to rename</small>
      </button>
      <button class="dispenser-btn" data-dispenser="5">
        <span id="dispenser-name-5">Dispenser 6</span><br>
        <small>Double-click to rename</small>
      </button>
    </div>
  </div>
  <div class="sequence-builder">
    <h3>🔧 Create New Sequence</h3>
    <div class="builder-container">
      <div class="sequence-name-input">
        <label for="sequence-name">Sequence Name:</label>
        <input type="text" id="sequence-name" placeholder="e.g., morning, evening, bedtime" />
      </div>
      <div class="dispenser-counts">
        <div class="count-item">
          <label id="count-label-0">Dispenser 1:</label>
          <div class="count-controls">
            <button class="count-btn" onclick="adjustCount(0, -1)">-</button>
            <span class="count-display" id="count-0">0</span>
            <button class="count-btn" onclick="adjustCount(0, 1)">+</button>
          </div>
        </div>
        <div class="count-item">
          <label id="count-label-1">Dispenser 2:</label>
          <div class="count-controls">
            <button class="count-btn" onclick="adjustCount(1, -1)">-</button>
            <span class="count-display" id="count-1">0</span>
            <button class="count-btn" onclick="adjustCount(1, 1)">+</button>
          </div>
        </div>
        <div class="count-item">
          <label id="count-label-2">Dispenser 3:</label>
          <div class="count-controls">
            <button class="count-btn" onclick="adjustCount(2, -1)">-</button>
            <span class="count-display" id="count-2">0</span>
            <button class="count-btn" onclick="adjustCount(2, 1)">+</button>
          </div>
        </div>
        <div class="count-item">
          <label id="count-label-3">Dispenser 4:</label>
          <div class="count-controls">
            <button class="count-btn" onclick="adjustCount(3, -1)">-</button>
            <span class="count-display" id="count-3">0</span>
            <button class="count-btn" onclick="adjustCount(3, 1)">+</button>
          </div>
        </div>
        <div class="count-item">
          <label id="count-label-4">Dispenser 5:</label>
          <div class="count-controls">
            <button class="count-btn" onclick="adjustCount(4, -1)">-</button>
            <span class="count-display" id="count-4">0</span>
            <button class="count-btn" onclick="adjustCount(4, 1)">+</button>
          </div>
        </div>
        <div class="count-item">
          <label id="count-label-5">Dispenser 6:</label>
          <div class="count-controls">
            <button class="count-btn" onclick="adjustCount(5, -1)">-</button>
            <span class="count-display" id="count-5">0</span>
            <button class="count-btn" onclick="adjustCount(5, 1)">+</button>
          </div>
        </div>
      </div>
      <div class="builder-actions">
        <button class="create-sequence-btn" onclick="createSequence()">✨ Create Sequence</button>
        <button class="reset-builder-btn" onclick="resetBuilder()">🔄 Reset</button>
      </div>
      <div class="sequence-preview">
        <strong>Preview:</strong> <span id="sequence-preview">SEQUENCE [name] (0,0,0,0,0,0)</span>
      </div>
    </div>
  </div>
  <div class="sequences">
    <h3>Pill Sequences:</h3>
    <div id="sequences-container">Loading sequences...</div>
    <button onclick="refreshSequences()" class="refresh-btn">🔄 Refresh Sequences</button>
  </div>
  <div class="main-content">
    <div class="terminal-section">
      <div id="log"></div>
      <input id="cmd" placeholder="Enter command...">
    </div>
    <div class="dispensing-log">
      <h3>Dispensing Log (24h)</h3>
      <div id="dispensing-log-container">
        <em>No dispensing activity yet...</em>
      </div>
      <button onclick="clearDispensingLog()" class="clear-log-btn">Clear Log</button>
    </div>
  </div>
  <script>
    // Generate or retrieve persistent device ID
    function getDeviceId() {
      let deviceId = localStorage.getItem('pillDeviceId');
      if (!deviceId) {
        deviceId = 'device_' + Math.random().toString(36).substr(2, 8);
        localStorage.setItem('pillDeviceId', deviceId);
        console.log('Generated new device ID:', deviceId);
      }
      return deviceId;
    }

    // Load dispenser names from localStorage
    function loadDispenserNames() {
      for (let i = 0; i < 6; i++) {
        let savedName = localStorage.getItem('dispenserName_' + i);
        if (savedName) {
          document.getElementById('dispenser-name-' + i).textContent = savedName;
        }
      }
    }

    // Setup dispenser button click handling
    function setupDispenserButtons() {
      let clickTimeout = null;
      
      document.querySelectorAll('.dispenser-btn').forEach(button => {
        button.addEventListener('click', function(e) {
          e.preventDefault();
          let dispenserIndex = parseInt(this.getAttribute('data-dispenser'));
          
          if (clickTimeout) {
            // Double click detected
            clearTimeout(clickTimeout);
            clickTimeout = null;
            renameDispenser(dispenserIndex);
          } else {
            // Single click - set timeout to check for double click
            clickTimeout = setTimeout(() => {
              dispensePill(dispenserIndex);
              clickTimeout = null;
            }, 300); // 300ms delay to detect double click
          }
        });
      });
    }

    // Dispense a single pill from specified dispenser
    function dispensePill(dispenserIndex) {
      let dispenserName = document.getElementById('dispenser-name-' + dispenserIndex).textContent;
      ws.send('START ' + (dispenserIndex + 1)); // START 1-6 for servos
      log.innerHTML += '<span style="color:#ffaa00;">Dispensing from: ' + dispenserName + ' (Servo ' + (dispenserIndex + 1) + ')</span><br>';
      log.scrollTop = log.scrollHeight;
      
      // Log the dispensing action
      addToDispensingLog('Single Dispense', dispenserName + ' (Servo ' + (dispenserIndex + 1) + ')');
    }

    // Rename a dispenser
    function renameDispenser(dispenserIndex) {
      let currentName = document.getElementById('dispenser-name-' + dispenserIndex).textContent;
      let newName = prompt('Enter new name for dispenser ' + (dispenserIndex + 1) + ':', currentName);
      if (newName && newName.trim() !== '') {
        newName = newName.trim();
        document.getElementById('dispenser-name-' + dispenserIndex).textContent = newName;
        localStorage.setItem('dispenserName_' + dispenserIndex, newName);
        log.innerHTML += '<span style="color:#00ffaa;">Renamed dispenser ' + (dispenserIndex + 1) + ' to: ' + newName + '</span><br>';
        log.scrollTop = log.scrollHeight;
        
        // Update builder labels
        updateBuilderLabels();
      }
    }

    // Dispensing log functions
    function addToDispensingLog(action, dispenserInfo) {
      let now = new Date();
      let logEntry = {
        timestamp: now.getTime(),
        deviceId: deviceId,
        action: action,
        dispenserInfo: dispenserInfo,
        time: now.toLocaleTimeString(),
        date: now.toLocaleDateString()
      };
      
      // Get existing log for this device
      let logKey = 'dispensingLog_' + deviceId;
      let existingLog = JSON.parse(localStorage.getItem(logKey) || '[]');
      
      // Add new entry
      existingLog.push(logEntry);
      
      // Remove entries older than 24 hours
      let twentyFourHoursAgo = now.getTime() - (24 * 60 * 60 * 1000);
      existingLog = existingLog.filter(entry => entry.timestamp > twentyFourHoursAgo);
      
      // Save back to localStorage
      localStorage.setItem(logKey, JSON.stringify(existingLog));
      
      // Update display
      updateDispensingLogDisplay();
    }

    function updateDispensingLogDisplay() {
      let logKey = 'dispensingLog_' + deviceId;
      let dispensingLog = JSON.parse(localStorage.getItem(logKey) || '[]');
      let container = document.getElementById('dispensing-log-container');
      
      if (dispensingLog.length === 0) {
        container.innerHTML = '<em>No dispensing activity yet...</em>';
        return;
      }
      
      // Sort by timestamp (newest first)
      dispensingLog.sort((a, b) => b.timestamp - a.timestamp);
      
      let html = '';
      dispensingLog.forEach(entry => {
        html += '<div class="log-entry">';
        html += '<div class="log-time">' + entry.time + '</div>';
        html += '<div class="log-action">' + entry.action + '</div>';
        html += '<div class="log-dispenser">' + entry.dispenserInfo + '</div>';
        html += '</div>';
      });
      
      container.innerHTML = html;
    }

    function clearDispensingLog() {
      if (confirm('Clear all dispensing log entries for this device?')) {
        let logKey = 'dispensingLog_' + deviceId;
        localStorage.removeItem(logKey);
        updateDispensingLogDisplay();
        log.innerHTML += '<span style="color:#ff6b6b;">Dispensing log cleared</span><br>';
        log.scrollTop = log.scrollHeight;
      }
    }

    // Sequence builder functions
    var counts = [0, 0, 0, 0, 0, 0];

    function adjustCount(index, change) {
      counts[index] = Math.max(0, Math.min(9, counts[index] + change));
      document.getElementById('count-' + index).textContent = counts[index];
      updateSequencePreview();
    }

    function updateSequencePreview() {
      let name = document.getElementById('sequence-name').value || '[name]';
      let countsStr = counts.join(',');
      document.getElementById('sequence-preview').textContent = 'SEQUENCE ' + name + ' (' + countsStr + ')';
    }

    function createSequence() {
      let name = document.getElementById('sequence-name').value.trim();
      if (!name) {
        alert('Please enter a sequence name');
        return;
      }
      
      let totalPills = counts.reduce((sum, count) => sum + count, 0);
      if (totalPills === 0) {
        alert('Please set at least one dispenser count greater than 0');
        return;
      }
      
      let command = 'SEQUENCE ' + deviceId + ' ' + name + ' (' + counts.join(',') + ')';
      ws.send(command);
      log.innerHTML += '<span style="color:#4caf50;">Creating sequence: ' + name + '</span><br>';
      log.scrollTop = log.scrollHeight;
      
      // Reset builder after creation
      setTimeout(resetBuilder, 1000);
    }

    function resetBuilder() {
      document.getElementById('sequence-name').value = '';
      counts = [0, 0, 0, 0, 0, 0];
      for (let i = 0; i < 6; i++) {
        document.getElementById('count-' + i).textContent = '0';
      }
      updateSequencePreview();
    }

    function updateBuilderLabels() {
      for (let i = 0; i < 6; i++) {
        let dispenserName = document.getElementById('dispenser-name-' + i).textContent;
        document.getElementById('count-label-' + i).textContent = dispenserName + ':';
      }
    }

    var deviceId = getDeviceId();
    var gateway = 'ws://' + window.location.hostname + ':81/';
    var ws = new WebSocket(gateway);
    var log = document.getElementById("log");
    var cmd = document.getElementById("cmd");
    var sequencesContainer = document.getElementById("sequences-container");
    var sequences = [];

    // Show device ID to user
    ws.onopen = function() {
      log.innerHTML += '<span style="color:#ffff00;">Device ID: ' + deviceId + '</span><br>';
      loadDispenserNames();
      setupDispenserButtons();
      updateBuilderLabels();
      updateSequencePreview();
      updateDispensingLogDisplay();
      refreshSequences();
      
      // Add event listener for sequence name input
      document.getElementById('sequence-name').addEventListener('input', updateSequencePreview);
    };

    ws.onmessage = function(evt) {
      let message = evt.data;
      log.innerHTML += message + "<br>";
      log.scrollTop = log.scrollHeight;
      
      // Parse sequence list from incoming messages
      if (message.includes('  - ')) {
        let seqName = message.split('  - ')[1];
        if (seqName && !sequences.includes(seqName.trim())) {
          sequences.push(seqName.trim());
          updateSequenceButtons();
        }
      }
      
      // Clear sequences list when starting a new LIST
      if (message.includes('[INFO] Sequences for device')) {
        sequences = [];
      }
      
      // Handle "No sequences found" case
      if (message.includes('[INFO] No sequences found for device')) {
        sequences = [];
        updateSequenceButtons();
      }
      
      // Auto-refresh sequences when operations complete
      if (message.includes('[SEQ] Stored sequence') || 
          message.includes('deleted successfully')) {
        setTimeout(function() {
          sequences = []; // Clear before refresh
          refreshSequences();
        }, 500);
      }
    };

    function refreshSequences() {
      ws.send('LIST ' + deviceId);
    }

    function updateSequenceButtons() {
      if (sequences.length === 0) {
        sequencesContainer.innerHTML = '<em>No sequences found. Create one with: SEQUENCE morning (1,1,0,2,0,6)</em>';
      } else {
        let html = '';
        for (let seqName of sequences) {
          html += '<button class="sequence-btn" onclick="executeSequence(\'' + seqName + '\')">' + seqName + '</button>';
          html += '<button class="sequence-btn delete-btn" onclick="deleteSequence(\'' + seqName + '\')">✕</button>';
        }
        sequencesContainer.innerHTML = html;
      }
    }

    function executeSequence(name) {
      ws.send('EXECUTE ' + deviceId + ' ' + name);
      log.innerHTML += '<span style="color:#00ffff;">Executing sequence: ' + name + '</span><br>';
      log.scrollTop = log.scrollHeight;
      
      // Log the sequence execution
      addToDispensingLog('Sequence Execute', 'Sequence: ' + name);
    }

    function deleteSequence(name) {
      if (confirm('Delete sequence "' + name + '"?')) {
        ws.send('DELETE ' + deviceId + ' ' + name);
      }
    }

    cmd.addEventListener("keydown", function(e) {
      if (e.key === "Enter") {
        let command = cmd.value.trim();
        
        // Auto-add device ID to commands that need it
        if (command.startsWith('SEQUENCE ') && !command.match(/SEQUENCE\s+device_\w+/)) {
          // Insert device ID after "SEQUENCE "
          command = command.replace(/^SEQUENCE\s+/, 'SEQUENCE ' + deviceId + ' ');
        }
        else if (command === 'LIST') {
          // Add device ID to LIST command
          command = 'LIST ' + deviceId;
        }
        else if (command.startsWith('EXECUTE ') && !command.match(/EXECUTE\s+device_\w+/)) {
          // Insert device ID after "EXECUTE "
          command = command.replace(/^EXECUTE\s+/, 'EXECUTE ' + deviceId + ' ');
        }
        else if (command.startsWith('DELETE ') && !command.match(/DELETE\s+device_\w+/)) {
          // Insert device ID after "DELETE "
          command = command.replace(/^DELETE\s+/, 'DELETE ' + deviceId + ' ');
        }
        
        ws.send(command);
        cmd.value = "";
      }
    });
  </script>
</body>
</html>
)rawliteral";
    return htmlPage;
}
