// src/ServoMotor.cpp
#include "ServoMotor.h"

// Static channel counter to assign unique PWM channels
int ServoMotor::channelCounter = 0;

ServoMotor::ServoMotor(int servoPin) : pin(servoPin), currentAngle(0) {
    pwmChannel = channelCounter++;
    if (channelCounter > 15) {  // ESP32 has 16 LEDC channels
        channelCounter = 0;  // Wrap around if needed
    }
}

void ServoMotor::initialize() {
    // Setup LEDC channel for this servo
    // 50Hz frequency, 16-bit resolution for maximum precision
    ledcSetup(pwmChannel, 50, 16);
    ledcAttachPin(pin, pwmChannel);
    
    Serial.println("[SERVO] Initialized pin " + String(pin) + " on PWM channel " + String(pwmChannel));
    
    // Move to default position
    moveAndRelease(Config::DEFAULT_START_ANGLE);
}

uint32_t ServoMotor::angleToLEDC(int angle) {
    // Convert angle (0-180°) to LEDC duty cycle
    // Many servos need wider range than standard 1ms-2ms
    // For 20ms period at 16-bit resolution (65535 counts):
    
    // Wider pulse range for better servo compatibility
    uint32_t minPulse = 1638;   // 0.5ms pulse width (0°)
    uint32_t maxPulse = 8192;   // 2.5ms pulse width (180°)
    
    // Linear interpolation for precise positioning
    uint32_t dutyCycle = minPulse + ((angle * (maxPulse - minPulse)) / 180);
    
    return dutyCycle;
}

void ServoMotor::moveAndRelease(int target) {
    // Constrain angle to valid range
    target = constrain(target, 0, 180);
    
    // Calculate precise duty cycle
    uint32_t dutyCycle = angleToLEDC(target);
    
    // Send PWM signal
    ledcWrite(pwmChannel, dutyCycle);
    
    // Hold position for specified time
    delay(Config::SERVO_DELAY_MS);
    
    // Release servo (stop PWM signal to save power)
    ledcWrite(pwmChannel, 0);
    
    // Update current position
    currentAngle = target;
    
    Serial.println("[SERVO] Pin " + String(pin) + " moved to " + String(target) + "° (duty: " + String(dutyCycle) + ")");
}

void ServoMotor::moveTo(int targetAngle) {
    moveAndRelease(targetAngle);
}

void ServoMotor::reset() {
    moveAndRelease(180);
}
