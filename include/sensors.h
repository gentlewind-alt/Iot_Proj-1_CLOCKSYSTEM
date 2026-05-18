#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

// === External variables ===
extern bool inIdleAnimation;
extern bool isIdlePlaying;
extern const unsigned long idleTimeout;
extern unsigned long lastMotionTime;

// Alarm related variables
extern volatile bool alarmBeeping;
extern bool running;        // Stopwatch state
extern bool alarmEditing;   // Alarm edit mode state

// === Function prototypes ===
void setupMotionSensor();
void checkForUserActivity();
void playScheduledAnimation();

#endif // SENSORS_H
