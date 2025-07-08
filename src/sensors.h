#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <RTClib.h>

// Forward declarations for types used
struct Animation;
extern const Animation* currentAnim;;
extern int currentFrame;

// Scheduler struct
struct AnimationSchedule {
    int startHour;
    int endHour;
    const Animation** anims;
    int numAnims;
};

extern bool pirState;  // Current state of the PIR sensor
extern unsigned long lastMotionTime;  // Last time motion was detected
extern bool inIdleAnimation;  // Whether the device is in idle animation mode
extern const unsigned long idleTimeout;  // Timeout for entering idle animation
extern const float motionThreshold;  // Threshold for detecting shaking
extern bool idleBlockedForAlarm;
extern float smoothX;  // Smoothed X-axis orientation
extern float smoothY;  // Smoothed Y-axis orientation
extern bool shaking;  // Whether the device is currently shaking
extern unsigned long shakeStart;  // Start time of the shaking
extern const unsigned long shakeDuration;  // Duration of the shake animation

extern AnimationSchedule schedules[];
extern const int NUM_SCHEDULES;

void setupMotionSensor();
long getDistanceCM();  // Function to get distance from ultrasonic sensor
void drawBitmapAt(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color);
void motionSensorLoop();  // Main loop for handling motion sensor events
void orienConfig();

#endif