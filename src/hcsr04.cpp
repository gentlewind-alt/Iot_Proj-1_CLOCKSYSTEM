#include "hcsr04.h"

DistanceSensor::DistanceSensor(uint8_t trig, uint8_t echo)
    : trigPin(trig), echoPin(echo) {
    // Initialize readings array
    for (int i = 0; i < SMOOTHING_SIZE; i++) {
        readings[i] = 0;
    }
}

void DistanceSensor::begin() {
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    digitalWrite(trigPin, LOW);
}

uint16_t DistanceSensor::getRawDistance() {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(5);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    
    // Measure pulse duration (timeout after 25ms = ~430cm)
    // On ESP32-C3, pulseIn is generally reliable with these timeouts
    long duration = pulseIn(echoPin, HIGH, 25000);
    
    if (duration <= 0) return 999;  // Timeout or error (return large value)
    
    // Distance = time * speed_of_sound / 2
    // speed_of_sound = 343 m/s = 0.0343 cm/μs
    uint16_t distance = (uint16_t)((duration * 0.0343) / 2.0);
    
    return (distance == 0) ? 999 : distance;
}

uint16_t DistanceSensor::getDistance() {
    // Throttle measurements to 60ms (standard HC-SR04 cycle)
    if (millis() - lastMeasureTime < 60) { 
        return cachedDistance;
    }
    lastMeasureTime = millis();
    
    // Get 3 quick samples for a median filter
    uint16_t samples[3];
    int validSamples = 0;
    
    for (int i = 0; i < 3; i++) {
        uint16_t r = getRawDistance();
        // Accept all readings, but mark timeouts as 999
        samples[i] = r;
        if (r < 400) validSamples++;
        
        if (i < 2) delay(10); // 10ms gap between pulses to avoid ghost echoes
    }
    
    uint16_t result;
    // Median of 3 (robust against single-sample noise)
    if ((samples[0] <= samples[1]) && (samples[0] <= samples[2])) {
        result = (samples[1] <= samples[2]) ? samples[1] : samples[2];
    } else if ((samples[1] <= samples[0]) && (samples[1] <= samples[2])) {
        result = (samples[0] <= samples[2]) ? samples[0] : samples[2];
    } else {
        result = (samples[0] <= samples[1]) ? samples[0] : samples[1];
    }

    // Apply low-pass filter (Exponential Moving Average)
    // If result is 999 (out of range), cachedDistance will slowly move out of range
    if (result == 999) {
        // Fast drift to out-of-range state to avoid "stuck" feel
        cachedDistance = (cachedDistance > 400) ? 999 : (uint16_t)(cachedDistance + 20);
    } else {
        // Standard smoothing for valid readings
        cachedDistance = (uint16_t)(result * 0.6 + cachedDistance * 0.4);
    }
    
    // Final clamp for safety
    if (cachedDistance > 1000) cachedDistance = 999;
    
    return cachedDistance;
}
