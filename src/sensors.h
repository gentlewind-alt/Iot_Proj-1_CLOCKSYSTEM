#ifndef SENSORS_H
#define SENSORS_H

extern bool pirState;  // Current state of the PIR sensor
extern unsigned long lastMotionTime;  // Last time motion was detected
extern bool inIdleAnimation;  // Whether the device is in idle animation mode
extern const unsigned long idleTimeout;  // Timeout for entering idle animation
extern float smoothX;  // Smoothed X-axis orientation
extern float smoothY;  // Smoothed Y-axis orientation
extern bool shaking;  // Whether the device is currently shaking
extern unsigned long shakeStart;  // Start time of the shaking
extern const float motionThreshold;  // Threshold for detecting shaking
extern bool idleBlockedForAlarm;
extern long getDistanceCM();  // Function to get distance from ultrasonic sensor
void motionSensorLoop();  // Main loop for handling motion sensor events
void setupMotionSensor();
void orienConfig();
void drawBitmapAt(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color);
extern Animation* currentAnim;  // Current animation being displayed
extern int currentFrame;  // Current frame index in the animation
#endif