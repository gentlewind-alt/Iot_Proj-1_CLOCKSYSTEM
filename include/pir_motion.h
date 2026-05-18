#ifndef PIR_MOTION_H
#define PIR_MOTION_H

#include <Arduino.h>

// PIR motion sensor with debouncing and state tracking
// Used for alarm motion-stop feature

class MotionDetector {
private:
    uint8_t pirPin;
    
    // Debouncing
    static constexpr unsigned long DEBOUNCE_INTERVAL = 500;  // 500ms debounce
    unsigned long lastMotionTime = 0;
    unsigned long lastStateChangeTime = 0;
    
    // State tracking
    volatile bool motionDetected = false;
    volatile bool lastRawState = false;
    
    // Motion event callback
    typedef void (*MotionCallback)(bool detected);
    MotionCallback onMotionChange = nullptr;
    
    // Throttling for repeated checks
    unsigned long lastCheckTime = 0;
    static constexpr unsigned long CHECK_INTERVAL = 50;  // Check every 50ms
    
public:
    MotionDetector(uint8_t pin);
    
    // Initialize PIR sensor
    void begin();
    
    // Update motion state (call frequently in loop)
    void update();
    
    // Check if motion currently detected (debounced)
    bool isMotionDetected();
    
    // Get raw PIR sensor reading (not debounced)
    bool getRawMotion();
    
    // Get time since last motion event (milliseconds)
    unsigned long timeSinceMotion();
    
    // Register callback for motion detection changes
    void onMotionStateChange(MotionCallback callback);
    
    // Reset motion detection state
    void reset();
};

#endif // PIR_MOTION_H
