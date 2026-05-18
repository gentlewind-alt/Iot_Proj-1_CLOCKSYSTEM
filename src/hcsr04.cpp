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
    delayMicroseconds(5); // Increased for stability
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    
    // Measure pulse duration (timeout after 20ms = ~340cm)
    // 10ms was a bit tight for many environments
    long duration = pulseIn(echoPin, HIGH, 20000);
    
    if (duration == 0 || duration > 25000) return 0;  // Out of range or timeout
    
    // Distance = time * speed_of_sound / 2
    // speed_of_sound = 343 m/s = 0.0343 cm/μs
    // Use floating point for precision then cast
    uint16_t distance = (uint16_t)((duration * 0.0343) / 2.0);
    
    return distance;
}

uint16_t DistanceSensor::getDistance() {
    // Throttle measurements slightly but keep it fast enough for gestures
    if (millis() - lastMeasureTime < 50) { 
        return cachedDistance;
    }
    lastMeasureTime = millis();
    
    // Get 3 quick samples for a median filter (excellent for rejecting spikes)
    uint16_t samples[3];
    int validSamples = 0;
    
    for (int i = 0; i < 3; i++) {
        uint16_t r = getRawDistance();
        if (r > 0 && r < 400) { // Reject 0 and extreme outliers
            samples[validSamples++] = r;
        }
        if (i < 2) delayMicroseconds(200); // Small gap between pulses
    }
    
    if (validSamples == 0) return cachedDistance; // Keep old value if all failed

    uint16_t result;
    if (validSamples == 1) {
        result = samples[0];
    } else if (validSamples == 2) {
        result = (samples[0] + samples[1]) / 2;
    } else {
        // Median of 3
        if ((samples[0] <= samples[1]) && (samples[0] <= samples[2])) {
            result = (samples[1] <= samples[2]) ? samples[1] : samples[2];
        } else if ((samples[1] <= samples[0]) && (samples[1] <= samples[2])) {
            result = (samples[0] <= samples[2]) ? samples[0] : samples[2];
        } else {
            result = (samples[0] <= samples[1]) ? samples[0] : samples[1];
        }
    }

    // Apply a light low-pass filter (Exponential Moving Average) 
    // to the median result for ultra-smoothness
    // alpha = 0.7 (70% new reading, 30% old)
    cachedDistance = (uint16_t)(result * 0.7 + cachedDistance * 0.3);
    
    return cachedDistance;
}
