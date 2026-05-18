#include "sensors.h"
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <FS.h>
#include <LittleFS.h>
#include <stdlib.h>
#include <time.h>
#include "interface.h"
#include "animation.h"  // ✅ Animation system

extern Adafruit_SSD1306 display;
extern Adafruit_MPU6050 mpu;

// Idle mode variables
const unsigned long idleTimeout = 120 * 1000; // 120s
unsigned long lastMotionTime = 0;
bool inIdleAnimation = false;
bool isIdlePlaying = false; 

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

void playRandomIdleAnimation() {
    if (globalAnimPlayer) {
        playRandomAnimation("/", 80, false);
    }
}

void playScheduledAnimation() {
    if (globalAnimPlayer) {
        if (!globalAnimPlayer->isRunning()) {
            playRandomIdleAnimation();
        } else {
            globalAnimPlayer->nextFrame();
        }
    }
}

// Configure PIR motion sensor pin and reset timers
void setupMotionSensor() {
  pinMode(PIR_PIN, INPUT);
  lastMotionTime = millis();
}

void checkForUserActivity() {
  static unsigned long lastActivityCheckTime = 0;
  const unsigned long ACTIVITY_CHECK_INTERVAL = 200;  // Check every 200ms
  
  if (millis() - lastActivityCheckTime < ACTIVITY_CHECK_INTERVAL) {
    return;
  }
  lastActivityCheckTime = millis();

  // Use the global sensor objects instead of local implementations
  bool pirVal = motionDetector ? motionDetector->getRawMotion() : digitalRead(PIR_PIN);
  uint16_t distance = distanceSensor ? distanceSensor->getDistance() : 0;
  bool buttonUsed = digitalRead(pinSW) == LOW || digitalRead(pinRST) == LOW || rotaryUsed;

  if (alarmBeeping && (pirVal || (distance > 0 && distance < 20))) {
    lastMotionTime = millis();
  }

  // If any interaction or significant movement detected
  if (buttonUsed || (pirVal) || (distance > 10 && distance < 100)) {
    lastMotionTime = millis();
    rotaryUsed = false; // Reset flag
    
    if (inIdleAnimation) {
      Serial.println("🔆 Activity detected. Waking up from idle.");
      inIdleAnimation = false;
      isIdlePlaying = false;
      display.clearDisplay();
      display.display();
    }
  }

  // Enter idle if timeout exceeded
  if (!inIdleAnimation && (millis() - lastMotionTime > idleTimeout)) {
    Serial.println("💤 Inactivity timeout. Entering idle mode.");
    inIdleAnimation = true;
    isIdlePlaying = false;
  }
}
