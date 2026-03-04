#include "sensors.h"
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <FS.h>
#include <SPIFFS.h>
#include <stdlib.h>
#include <time.h>
#include "interface.h"

extern Adafruit_SSD1306 display;
extern Adafruit_MPU6050 mpu;

// Idle mode variables
const unsigned long idleTimeout = 120 * 1000; // 20s
float smoothX = 0, smoothY = 0;
bool shaking = false;
bool pirState = false;
unsigned long lastMotionTime = 0;
bool inIdleAnimation = false;
bool isIdlePlaying = false; 
unsigned long lastInteractionTime = 0;

// PIR + motion constants
const float motionThreshold = 2.5;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define FRAME_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 8)

// Packed animation player
static bool readPackedHeader(File &f, uint16_t &w, uint16_t &h, uint32_t &count) {
  char magic[8];
  if (f.readBytes(magic, 8) != 8) return false;
  if (memcmp(magic, "BINPACK\0", 8) != 0) return false;
  if (f.readBytes((char*)&w, 2) != 2) return false;
  if (f.readBytes((char*)&h, 2) != 2) return false;
  if (f.readBytes((char*)&count, 4) != 4) return false;
  return true;
}

void playPackedAnimation(const char* path, uint16_t frameDelayMs) {
  if (!SPIFFS.exists(path)) {
    Serial.printf("❌ Animation file missing: %s\n", path);
    return;
  }
  File f = SPIFFS.open(path, "r");
  if (!f) {
    Serial.printf("❌ Failed to open: %s\n", path);
    return;
  }
  uint16_t w = 0, h = 0; uint32_t count = 0;
  if (!readPackedHeader(f, w, h, count)) {
    Serial.printf("❌ Invalid packed animation: %s\n", path);
    f.close();
    return;
  }
  const size_t bytesPerFrame = (w * h) / 8;
  static uint8_t frame[FRAME_SIZE];
  for (uint32_t i = 0; i < count && inIdleAnimation; i++) {
    size_t n = f.read(frame, bytesPerFrame);
    if (n != bytesPerFrame) break;
    display.clearDisplay();
    display.drawBitmap(0, 0, frame, SCREEN_WIDTH, SCREEN_HEIGHT, 1);
    display.display();
    delay(frameDelayMs);
    checkForUserActivity();
  }
  f.close();
}

// Idle animation list
const char* idleAnimations[] = {
  "/happy_2.pbin", "/demon_slayer.pbin", "/yawn.pbin", "/rain.pbin", "/cry2.pbin", "/love.pbin", "/cry.pbin",
  "/rainbow.pbin", "/sakura.pbin", "/sneeze_2.pbin", "/xi_khoi.pbin", "/squint.pbin", "/angry2.pbin",
  "/dizzy.pbin", "/headlight_ex.pbin", "/smile.pbin", "/up_size_down.pbin", "/sung_nuoc_2.pbin", "/angry3.pbin",
  "/scared.pbin", "/UwU.pbin", "/police.pbin", "/devil_eyes.pbin", "/look_around.pbin", "/neon.pbin",
  "/thong_long.pbin", "/egg.pbin", "/thu_nho.pbin", "/music.pbin", "/surprise_look.pbin", "/smirk.pbin",
  "/gian_du.pbin", "/dasai2.pbin", "/love2.pbin", "/samurai.pbin", "/yakura.pbin", "/devil.pbin",
  "/xi_lua.pbin", "/keep_it_up.pbin", "/nervous_laugh.pbin", "/sleepy.pbin", "/wheels.pbin", "/mochi.pbin",
  "/angry.pbin", "/pong.pbin", "/turbo.pbin", "/headlight.pbin", "/toc_do.pbin", "/devil_2.pbin", "star.pbin"
};

void setupMotionSensor() {
  pinMode(PIR_PIN, INPUT);
  lastMotionTime = millis();
}

long getDistanceCM() {
  const int samples = 5;
  long readings[samples];
  for (int i = 0; i < samples; i++) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    readings[i] = duration * 0.034 / 2;
    delay(10);
  }
  for (int i = 0; i < samples - 1; i++) {
    for (int j = i + 1; j < samples; j++) {
      if (readings[j] < readings[i]) {
        long t = readings[i]; readings[i] = readings[j]; readings[j] = t;
      }
    }
  }
  return readings[samples/2];
}

void checkForUserActivity() {
  bool pirVal = digitalRead(PIR_PIN);
  long distance = getDistanceCM();
  bool buttonUsed = digitalRead(pinSW) == LOW || digitalRead(pinRST) == LOW;

  if (alarmBeeping && pirVal && distance > 0 && distance < 10) {
    Serial.println("😴 Snooze triggered by motion <10cm!");
    alarmBeeping = false;
    snoozeUntil = millis() + SNOOZE_DURATION;
  }

  if (buttonUsed || running || alarmEditing) {
    lastMotionTime = millis();
    if (inIdleAnimation) {
      Serial.println("✨ Exiting idle mode via rotary/button.");
      inIdleAnimation = false;
      isIdlePlaying = false;
      display.clearDisplay();
      display.display();
    }
  }

  if (pirVal && distance > 30 && distance < 500) {
    if (!pirState) {
      Serial.println("🚶 Motion detected!");
      pirState = true;
    }
    lastMotionTime = millis();
    if (inIdleAnimation) {
      Serial.println("✨ Exiting idle mode due to PIR/Ultrasonic motion.");
      inIdleAnimation = false;
      isIdlePlaying = false;
      display.clearDisplay();
      display.display();
    }
  } else if (pirState && millis() - lastMotionTime > 3000) {
    Serial.println("🛑 PIR motion ended!");
    pirState = false;
  }

  if (!inIdleAnimation && (millis() - lastMotionTime > idleTimeout)) {
    Serial.println("💤 Timeout passed. Entering idle mode.");
    inIdleAnimation = true;
    isIdlePlaying = false;
  }
}

void playRandomIdleAnimation() {
  int total = sizeof(idleAnimations) / sizeof(idleAnimations[0]);
  if (total == 0) return;
  srand(millis());
  int choice = rand() % total;
  playPackedAnimation(idleAnimations[choice], 80);
}

void playScheduledAnimation() {
  checkForUserActivity();
  if (!inIdleAnimation || shaking) return;
  if (!isIdlePlaying) {
    Serial.println("💤 Entering idle animation loop");
    isIdlePlaying = true;
  }
  int animIndex = random(0, sizeof(idleAnimations) / sizeof(idleAnimations[0]));
  Serial.printf("🎞 Playing idle animation: %s\n", idleAnimations[animIndex]);
  playPackedAnimation(idleAnimations[animIndex], 80);
  Serial.println("✅ Animation cycle done");
  isIdlePlaying = false;
}
