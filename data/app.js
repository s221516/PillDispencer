// WebSocket connection
const ws = new WebSocket(`ws://${window.location.hostname}:81`);
const log = document.getElementById('log');
const cmd = document.getElementById('cmd');
const dispensersContainer = document.getElementById('dispensers');
const sequencesContainer = document.getElementById('sequences-container');

// Device and state management
let deviceId = '';
let sequences = [];
let isParsingSequenceList = false;
let dispenserNames = ['Dispenser 1', 'Dispenser 2'];
let dispensingLog = [];

// Initialize
function init() {
  deviceId = getDeviceId();
  document.getElementById('device-id-display').textContent = deviceId;
  loadDispenserNames();
  loadDispensingLog();
  setupDispenserButtons();
  updateBuilderLabels();
  updateSequencePreview();
  updateDispensingLogDisplay();
  refreshSequences();
  
  // Add event listener for sequence name input
  document.getElementById('sequence-name').addEventListener('input', updateSequencePreview);
}

function getDeviceId() {
  let stored = localStorage.getItem('deviceId');
  if (!stored) {
    // Create a more unique device ID using timestamp + random
    const timestamp = Date.now().toString(36);
    const random = Math.random().toString(36).substr(2, 5);
    stored = `device_${timestamp}_${random}`;
    localStorage.setItem('deviceId', stored);
    console.log('Generated new device ID:', stored);
  }
  console.log('Using device ID:', stored);
  return stored;
}

function loadDispenserNames() {
  const saved = localStorage.getItem('dispenserNames_' + deviceId);
  if (saved) {
    dispenserNames = JSON.parse(saved);
  }
  console.log('Loaded dispenser names:', dispenserNames);
}

function setupDispenserButtons() {
  let html = '';
  for (let i = 0; i < 2; i++) {
    html += `<button class="dispenser-btn" data-index="${i}" onpointerdown="startLongPress(event, ${i})" onpointerup="endLongPress(event)" onpointerleave="endLongPress(event)" onpointercancel="endLongPress(event)">${dispenserNames[i]}</button>`;
  }
  dispensersContainer.innerHTML = html;
}

function dispensePill(dispenserIndex) {
  console.log('=== DISPENSE PILL FUNCTION ===');
  console.log('Input servo number:', dispenserIndex);
  console.log('Dispenser names array:', dispenserNames);
  console.log('Looking up name at index:', dispenserIndex - 1);
  console.log('Dispenser name found:', dispenserNames[dispenserIndex - 1]);
  
  // Validate dispenser index
  if (dispenserIndex < 1 || dispenserIndex > 6) {
    console.error('*** ERROR: Invalid dispenser index:', dispenserIndex);
    return;
  }
  
  const message = `PILL ${dispenserIndex}`;
  console.log('Sending WebSocket message:', message);
  ws.send(message);
  
  const dispenserName = dispenserNames[dispenserIndex - 1] || `Dispenser ${dispenserIndex}`;
  const logMessage = `Dispensing from: ${dispenserName} (Servo ${dispenserIndex})`;
  console.log('Log message:', logMessage);
  
  log.innerHTML += `<span style="color:#00ffff;">${logMessage}</span><br>`;
  log.scrollTop = log.scrollHeight;
  
  addToDispensingLog('Manual Dispense', `${dispenserName} (Servo ${dispenserIndex})`);
}

function renameDispenser(dispenserIndex) {
  const currentName = dispenserNames[dispenserIndex];
  const newName = prompt(`Rename "${currentName}" to:`, currentName);
  
  if (newName && newName.trim() !== '' && newName !== currentName) {
    dispenserNames[dispenserIndex] = newName.trim();
    localStorage.setItem('dispenserNames_' + deviceId, JSON.stringify(dispenserNames));
    setupDispenserButtons();
    updateBuilderLabels();
    updateSequencePreview();
    
    addToDispensingLog('Rename', `"${currentName}" → "${newName}"`);
  }
}

// Long press functionality
let longPressTimer = null;
let longPressTarget = -1;
let isLongPressing = false;
let longPressTriggered = false;

function startLongPress(event, dispenserIndex) {
  event.preventDefault();
  
  console.log('=== BUTTON PRESSED ===');
  console.log('Dispenser index (0-based):', dispenserIndex);
  console.log('Servo number (1-based):', dispenserIndex + 1);
  console.log('Dispenser name:', dispenserNames[dispenserIndex]);
  
  isLongPressing = true;
  longPressTriggered = false;
  longPressTarget = dispenserIndex;
  
  console.log('Starting long press timer...');
  
  longPressTimer = setTimeout(() => {
    if (isLongPressing) {
      longPressTriggered = true;
      console.log('*** LONG PRESS TRIGGERED ***');
      
      // Provide haptic feedback if available
      if (navigator.vibrate) {
        navigator.vibrate(50);
      }
      
      // Visual feedback
      const button = event.target;
      const originalColor = button.style.backgroundColor;
      button.style.backgroundColor = '#ff6b6b';
      button.style.transform = 'scale(0.95)';
      
      setTimeout(() => {
        button.style.backgroundColor = originalColor;
        button.style.transform = 'scale(1)';
      }, 200);
      
      renameDispenser(dispenserIndex);
    }
  }, 600); // Reduced to 600ms for better responsiveness
}

function endLongPress(event) {
  console.log('=== BUTTON RELEASED ===');
  console.log('Was long pressing:', isLongPressing);
  console.log('Long press triggered:', longPressTriggered);
  console.log('Target dispenser (0-based):', longPressTarget);
  
  const wasLongPress = longPressTriggered;
  
  if (longPressTimer) {
    clearTimeout(longPressTimer);
    longPressTimer = null;
    console.log('Cleared long press timer');
  }
  
  if (isLongPressing && !longPressTriggered) {
    // This was a short press, trigger dispense
    const servoNumber = longPressTarget + 1;
    console.log('*** SHORT PRESS DETECTED ***');
    console.log('About to dispense from servo:', servoNumber);
    console.log('Dispenser name:', dispenserNames[longPressTarget]);
    
    setTimeout(() => {
      dispensePill(servoNumber);
    }, 50);
  }
  
  isLongPressing = false;
  longPressTriggered = false;
  longPressTarget = -1;
}

// Dispensing log functions
function addToDispensingLog(action, dispenserInfo) {
  const timestamp = new Date().toLocaleTimeString();
  const logEntry = {
    timestamp: timestamp,
    action: action,
    dispenser: dispenserInfo
  };
  
  dispensingLog.unshift(logEntry);
  if (dispensingLog.length > 50) {
    dispensingLog = dispensingLog.slice(0, 50);
  }
  
  localStorage.setItem('dispensingLog_' + deviceId, JSON.stringify(dispensingLog));
  updateDispensingLogDisplay();
}

function updateDispensingLogDisplay() {
  const logContainer = document.getElementById('dispensing-log');
  if (dispensingLog.length === 0) {
    logContainer.innerHTML = '<em>No dispensing activity yet</em>';
  } else {
    let html = '';
    for (let entry of dispensingLog) {
      let icon = '';
      switch(entry.action) {
        case 'Manual Dispense': icon = '💊'; break;
        case 'Sequence Execute': icon = '📋'; break;
        case 'Rename': icon = '✏️'; break;
        default: icon = '•';
      }
      html += `<div style="margin-bottom: 5px;"><span style="color: #4fc3f7;">[${entry.timestamp}]</span> ${icon} ${entry.action}: ${entry.dispenser}</div>`;
    }
    logContainer.innerHTML = html;
  }
}

function clearDispensingLog() {
  if (confirm('Clear all dispensing log entries?')) {
    dispensingLog = [];
    localStorage.removeItem('dispensingLog_' + deviceId);
    updateDispensingLogDisplay();
  }
}

// Sequence builder functions
let sequenceCounts = [0, 0];

function adjustCount(index, change) {
  sequenceCounts[index] = Math.max(0, sequenceCounts[index] + change);
  // Update the display
  document.getElementById(`count-${index}`).textContent = sequenceCounts[index];
  updateSequencePreview();
}

function updateSequencePreview() {
  const name = document.getElementById('sequence-name').value.trim();
  const previewText = document.getElementById('preview-text');
  
  if (name === '') {
    previewText.textContent = 'Enter a name to see preview';
    return;
  }
  
  const command = `SEQUENCE ${deviceId} ${name} (${sequenceCounts.join(',')})`;
  previewText.textContent = command;
}

// Prevent rapid sequence creation
let isCreatingSequence = false;

function createSequence() {
  if (isCreatingSequence) {
    console.log('Already creating a sequence, please wait...');
    return;
  }
  
  isCreatingSequence = true;
  
  const name = document.getElementById('sequence-name').value.trim();
  
  console.log('=== CREATING SEQUENCE ===');
  console.log('Device ID:', deviceId);
  console.log('Sequence name:', name);
  console.log('Current sequences for this device:', sequences);
  
  if (name === '') {
    alert('Please enter a sequence name');
    isCreatingSequence = false;
    return;
  }
  
  // Check for duplicate sequence names
  if (sequences.includes(name)) {
    alert(`A sequence named "${name}" already exists. Please choose a different name.`);
    isCreatingSequence = false;
    return;
  }
  
  if (sequenceCounts.every(count => count === 0)) {
    alert('Please set at least one dispenser count');
    isCreatingSequence = false;
    return;
  }
  
  // Add to local sequences immediately for instant feedback
  sequences.push(name);
  updateSequenceButtons();
  
  // Show immediate feedback in log
  log.innerHTML += `<span style="color:#ffff00;">Creating sequence: ${name}...</span><br>`;
  log.scrollTop = log.scrollHeight;
  
  const command = `SEQUENCE ${deviceId} ${name} (${sequenceCounts.join(',')})`;
  console.log('Sending sequence command:', command);
  ws.send(command);
  
  resetBuilder();
  
  // Reset the flag after a short delay
  setTimeout(() => {
    isCreatingSequence = false;
  }, 1000);
}

function resetBuilder() {
  sequenceCounts = [0, 0];
  document.getElementById('sequence-name').value = '';
  updateBuilderLabels(); // This will refresh the count displays
  updateSequencePreview();
}

function updateBuilderLabels() {
  let html = '';
  for (let i = 0; i < 2; i++) {
    html += `
      <div class="count-control">
        <span>${dispenserNames[i]}:</span>
        <div>
          <button type="button" onclick="adjustCount(${i}, -1)">-</button>
          <span id="count-${i}" style="margin: 0 10px; font-weight: bold;">${sequenceCounts[i]}</span>
          <button type="button" onclick="adjustCount(${i}, 1)">+</button>
        </div>
      </div>
    `;
  }
  document.getElementById('sequence-controls').innerHTML = html;
}

// WebSocket event handlers
ws.onopen = function() {
  log.innerHTML += '<span style="color:#00ff00;">Connected to ESP32</span><br>';
  log.scrollTop = log.scrollHeight;
  
  loadDispensingLog();
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
  
  // Only process sequence list messages meant for THIS device
  if (message.includes(`[INFO] Sequences for device ${deviceId}`)) {
    console.log('Starting fresh sequence list for OUR device:', deviceId);
    sequences = [];
    isParsingSequenceList = true;
}
  
  // Handle "No sequences found" ONLY for our device
  if (message.includes(`[INFO] No sequences found for device ${deviceId}`)) {
    console.log('No sequences found for our device, clearing list');
    sequences = [];
    isParsingSequenceList = false;
    updateSequenceButtons();
  }
  
  // Parse sequence list ONLY during active LIST response for OUR device
  if (isParsingSequenceList && message.includes('  - ')) {
    let seqName = message.split('  - ')[1];
    if (seqName && seqName.trim()) {
      const cleanName = seqName.trim();
      if (!sequences.includes(cleanName)) {
        console.log('Found sequence for our device:', cleanName);
        sequences.push(cleanName);
      }
    }
  }
  
  // End of sequence list (when we get a non-sequence message after starting)
  if (isParsingSequenceList && !message.includes('  - ') && !message.includes(`[INFO] Sequences for device ${deviceId}`)) {
    console.log('Finished parsing sequence list for our device, found:', sequences);
    isParsingSequenceList = false;
    updateSequenceButtons();
  }
  
  // Auto-refresh sequences when operations complete FOR OUR DEVICE
  if (message.includes(`[SEQ] Stored sequence`) && message.includes(`for device ${deviceId}`)) {
    console.log('Sequence stored successfully for our device, updating display');
    // Don't auto-refresh since we already added it locally
    // Just log the success
    log.innerHTML += `<span style="color:#00ff00;">✓ Sequence stored successfully</span><br>`;
    log.scrollTop = log.scrollHeight;
  }
  
  if (message.includes('deleted successfully') && message.includes(`for device ${deviceId}`)) {
    console.log('Auto-refreshing sequences after deletion for our device');
    setTimeout(function() {
      refreshSequences();
    }, 100);
  }
};

function refreshSequences() {
  console.log('Refreshing sequences for device:', deviceId);
  sequences = []; // Clear existing sequences
  isParsingSequenceList = false; // Reset parsing state
  ws.send('LIST ' + deviceId);
}

function updateSequenceButtons() {
  if (sequences.length === 0) {
    sequencesContainer.innerHTML = '<em>No sequences found. Create one with the builder below.</em>';
  } else {
    let html = '';
    for (let seqName of sequences) {
      html += `<button class="sequence-btn" onclick="executeSequence('${seqName}')">${seqName}</button>`;
      html += `<button class="sequence-btn delete-btn" onclick="deleteSequence('${seqName}')">✕</button>`;
    }
    sequencesContainer.innerHTML = html;
  }
}

function executeSequence(name) {
  ws.send('EXECUTE ' + deviceId + ' ' + name);
  log.innerHTML += `<span style="color:#00ffff;">Executing sequence: ${name}</span><br>`;
  log.scrollTop = log.scrollHeight;
  
  // Log the sequence execution
  addToDispensingLog('Sequence Execute', 'Sequence: ' + name);
}

function deleteSequence(name) {
  if (confirm(`Delete sequence "${name}"?`)) {
    console.log('Deleting sequence:', name, 'for device:', deviceId);
    
    // Remove from local array immediately for instant feedback
    const index = sequences.indexOf(name);
    if (index > -1) {
      sequences.splice(index, 1);
      updateSequenceButtons();
    }
    
    // Show immediate feedback in log
    log.innerHTML += `<span style="color:#ff8800;">Deleting sequence: ${name}...</span><br>`;
    log.scrollTop = log.scrollHeight;
    
    ws.send('DELETE ' + deviceId + ' ' + name);
  }
}

function loadDispensingLog() {
  const saved = localStorage.getItem('dispensingLog_' + deviceId);
  if (saved) {
    dispensingLog = JSON.parse(saved);
  }
}

// Command input handler
cmd.addEventListener("keydown", function(e) {
  if (e.key === "Enter") {
    let command = cmd.value.trim();
    
    // Don't send empty commands
    if (command === "") {
      cmd.value = "";
      return;
    }
    
    // Auto-add device ID to commands that need it
    if (command.startsWith('SEQUENCE ') && !command.match(/SEQUENCE\s+device_\w+/)) {
      command = command.replace(/^SEQUENCE\s+/, 'SEQUENCE ' + deviceId + ' ');
    }
    else if (command === 'LIST') {
      command = 'LIST ' + deviceId;
    }
    else if (command.startsWith('EXECUTE ') && !command.match(/EXECUTE\s+device_\w+/)) {
      command = command.replace(/^EXECUTE\s+/, 'EXECUTE ' + deviceId + ' ');
    }
    else if (command.startsWith('DELETE ') && !command.match(/DELETE\s+device_\w+/)) {
      command = command.replace(/^DELETE\s+/, 'DELETE ' + deviceId + ' ');
    }
    
    ws.send(command);
    cmd.value = "";
  }
});

// Initialize when page loads
window.onload = init;
