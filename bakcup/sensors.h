#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <RTClib.h>
#include "interface.h"

// Forward declarations for types used
struct Animation;
extern const Animation* currentAnim;;
extern int currentFrame;
struct Emote {
  const uint8_t* data;
  size_t size;
};

// Scheduler struct
struct AnimationSchedule {
    int startHour;
    int endHour;
    const Emote* anims;
    size_t count; 
};

extern bool isIdlePlaying;  // Whether an idle animation is currently playing
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
extern const AnimationSchedule schedules[];
extern const int NUM_SCHEDULES;
extern 
void GIFDraw(GIFDRAW *pDraw);
void setupMotionSensor();

long getDistanceCM();  // Function to get distance from ultrasonic sensor
void playGIFFrame(const uint8_t* frameData, size_t frameSize);
// void orienConfig();
void playScheduledAnimation();

#endif