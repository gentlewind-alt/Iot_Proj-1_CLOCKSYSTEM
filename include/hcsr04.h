#ifndef HCSR04_H
#define HCSR04_H

#include <Arduino.h>
#include <stdint.h>

// HC-SR04 ultrasonic distance sensor
// Pins: TRIG=GPIO18, ECHO=GPIO19

class DistanceSensor {
private:
    uint8_t trigPin;
    uint8_t echoPin;
    static constexpr uint16_t SMOOTHING_SIZE = 5;  // Moving average filter
    uint16_t readings[SMOOTHING_SIZE];
    uint8_t readingIndex = 0;
    uint32_t readSum = 0;
    unsigned long lastMeasureTime = 0;
    static constexpr unsigned long MEASURE_INTERVAL = 100;  // Measure every 100ms max
    uint16_t cachedDistance = 0;  // cm
    
public:
    DistanceSensor(uint8_t trig, uint8_t echo);
    
    // Initialize sensor pins
    void begin();
    
    // Get distance in cm (throttled, uses moving average)
    uint16_t getDistance();
    
    // Raw measurement (unfiltered)
    uint16_t getRawDistance();
    
    // Get smoothed distance (moving average)
    uint16_t getSmoothedDistance() const { return cachedDistance; }
    
    // Check if distance is in valid range
    static bool isInRange(uint16_t distance) {
        return distance > 0 && distance < 100;  // Valid: 0-100cm
    }
};

#endif // HCSR04_H
