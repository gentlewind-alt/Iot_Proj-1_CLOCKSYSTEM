#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

// === Pin definitions ===
// Make sure these match your actual wiring
// #define PIR_PIN    4    // PIR motion sensor
// #define TRIG_PIN   5    // Ultrasonic trigger
// #define ECHO_PIN   6    // Ultrasonic echo
// #define pinSW      7    // Navigation button (OK/Select)
// #define pinRST     8    // Reset/Back button

// === External variables ===
extern bool inIdleAnimation;
extern bool isIdlePlaying;
extern bool shaking;
extern const unsigned long idleTimeout;
extern unsigned long lastMotionTime;

// Alarm related variables (declared in clock.cpp or main.cpp)
extern volatile bool alarmBeeping;
extern unsigned long snoozeUntil;
extern const unsigned long SNOOZE_DURATION;
extern bool running;        // Stopwatch state
extern bool alarmEditing;   // Alarm edit mode state

// === Function prototypes ===
void setupMotionSensor();
long getDistanceCM();
void checkForUserActivity();
void playScheduledAnimation();

#endif
