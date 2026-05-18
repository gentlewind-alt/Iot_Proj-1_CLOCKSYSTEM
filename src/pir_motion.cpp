#include "pir_motion.h"

MotionDetector::MotionDetector(uint8_t pin) : pirPin(pin) {
    // Constructor stores PIR sensor pin
}

void MotionDetector::begin() {
    // Initialize PIR sensor pin as input
    pinMode(pirPin, INPUT);
    
    // Wait for PIR sensor to stabilize (typically 30-60 seconds for HC-SR501)
    // For now, just reset internal state
    motionDetected = false;
    lastRawState = false;
    lastMotionTime = millis();
    lastStateChangeTime = millis();
    lastCheckTime = millis();
}

void MotionDetector::update() {
    // Check if enough time has passed since last update
    unsigned long now = millis();
    if (now - lastCheckTime < CHECK_INTERVAL) {
        return;
    }
    lastCheckTime = now;
    
    // Read raw PIR sensor
    bool currentRawState = digitalRead(pirPin);
    
    // Only process if raw state has changed
    if (currentRawState != lastRawState) {
        lastStateChangeTime = now;
        lastRawState = currentRawState;
        return;  // Wait for debounce interval before accepting
    }
    
    // Check if debounce interval has expired
    if (now - lastStateChangeTime < DEBOUNCE_INTERVAL) {
        return;  // Still within debounce window
    }
    
    // Debounce interval passed, update official state
    if (currentRawState != motionDetected) {
        motionDetected = currentRawState;
        lastMotionTime = now;
        
        // Trigger callback if registered
        if (onMotionChange != nullptr) {
            onMotionChange(motionDetected);
        }
    }
}

bool MotionDetector::isMotionDetected() {
    // Trigger update to get latest debounced state
    update();
    return motionDetected;
}

bool MotionDetector::getRawMotion() {
    // Return raw sensor reading without debouncing
    return digitalRead(pirPin);
}

unsigned long MotionDetector::timeSinceMotion() {
    // Return milliseconds since last motion event
    if (!motionDetected && lastMotionTime > 0) {
        return millis() - lastMotionTime;
    }
    return 0;  // Motion currently active (or never detected)
}

void MotionDetector::onMotionStateChange(MotionCallback callback) {
    // Register callback function for motion state changes
    onMotionChange = callback;
}

void MotionDetector::reset() {
    // Reset motion detection state
    motionDetected = false;
    lastMotionTime = millis();
    lastStateChangeTime = millis();
    lastRawState = false;
}
