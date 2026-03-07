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

// Animation metadata: {name, number of frames, delay between frames in ms}
struct AnimationFrame {
  const char* name;
  int frameCount;      // Will be updated dynamically
  uint16_t delayMs;
};

// Animation definitions with initial frame counts (will be overwritten)
AnimationFrame idleAnimations[] = {
  {"happy_2", 0, 80},
  {"demon_slayer", 0, 80},
  {"yawn", 0, 80},
  {"rain", 0, 80},
  {"cry2", 0, 80},
  {"love", 0, 80},
  {"cry", 0, 80},
  {"rainbow", 0, 80},
  {"sakura", 0, 80},
  {"sneeze_2", 0, 80},
  {"xi_khoi", 0, 80},
  {"squint", 0, 80},
  {"angry2", 0, 80},
  {"dizzy", 0, 80},
  {"headlight_ex", 0, 80},
  {"smile", 0, 80},
  {"up_size_down", 0, 80},
  {"sung_nuoc_2", 0, 80},
  {"angry3", 0, 80},
  {"scared", 0, 80},
  {"UwU", 0, 80},
  {"police", 0, 80},
  {"devil_eyes", 0, 80},
  {"look_around", 0, 80},
  {"neon", 0, 80},
  {"thong_long", 0, 80},
  {"egg", 0, 80},
  {"thu_nho", 0, 80},
  {"music", 0, 80},
  {"surprise_look", 0, 80},
  {"smirk", 0, 80},
  {"gian_du", 0, 80},
  {"dasai2", 0, 80},
  {"love2", 0, 80},
  {"samurai", 0, 80},
  {"yakura", 0, 80},
  {"devil", 0, 80},
  {"xi_lua", 0, 80},
  {"keep_it_up", 0, 80},
  {"nervous_laugh", 0, 80},
  {"sleepy", 0, 80},
  {"wheels", 0, 80},
  {"mochi", 0, 80},
  {"angry", 0, 80},
  {"pong", 0, 80},
  {"turbo", 0, 80},
  {"headlight", 0, 80},
  {"toc_do", 0, 80},
  {"devil_2", 0, 80},
  {"star", 0, 80}
};

const int totalAnimations = sizeof(idleAnimations) / sizeof(idleAnimations[0]);

// Count .pbin files in a folder to determine frame count
// === DISABLED for Wokwi simulation (no SPIFFS support) ===
int countFramesInFolder(const char* folderName) {
  // char folderPath[64];
  // snprintf(folderPath, sizeof(folderPath), "/data/%s", folderName);
  // 
  // File folder = SPIFFS.open(folderPath);
  // if (!folder || !folder.isDirectory()) {
  //   Serial.printf("⚠️ Folder not found: %s\n", folderPath);
  //   return 0;
  // }
  // 
  // int frameCount = 0;
  // File file = folder.openNextFile();
  // while (file) {
  //   if (!file.isDirectory()) {
  //     frameCount++;
  //   }
  //   file = folder.openNextFile();
  // }
  // folder.close();
  
  return 0;  // Return 0 frames for simulation
}

// Scan all animation folders and update frame counts dynamically
// === DISABLED for Wokwi simulation (no SPIFFS support) ===
void initializeAnimationFrameCounts() {
  // Serial.println("\n🎬 Scanning animation folders...");
  // for (int i = 0; i < totalAnimations; i++) {
  //   int frames = countFramesInFolder(idleAnimations[i].name);
  //   idleAnimations[i].frameCount = frames;
  //   Serial.printf("  %s: %d frames\n", idleAnimations[i].name, frames);
  // }
  // Serial.println("✅ Animation frame counts updated dynamically\n");
  Serial.println("⚠️ Animation frame counts disabled (SPIFFS not available in simulation)");
}

// Load and play animation from frame folder (e.g., /data/angry/)
// === DISABLED for Wokwi simulation (no SPIFFS support) ===
void playAnimationFromFrames(const char* folderName, int frameCount, uint16_t frameDelayMs) {
  // char framePath[64];
  // static uint8_t frame[FRAME_SIZE];
  // 
  // Serial.printf("🎬 Playing animation: %s (%d frames)\n", folderName, frameCount);
  // 
  // for (int i = 0; i < frameCount && inIdleAnimation; i++) {
  //   // Build path: /data/angry/0.pbin, /data/angry/1.pbin, etc.
  //   snprintf(framePath, sizeof(framePath), "/data/%s/%d.pbin", folderName, i);
  //   
  //   if (!SPIFFS.exists(framePath)) {
  //     Serial.printf("❌ Frame missing: %s\n", framePath);
  //     break;
  //   }
  //   
  //   File f = SPIFFS.open(framePath, "r");
  //   if (!f) {
  //     Serial.printf("❌ Failed to open frame: %s\n", framePath);
  //     break;
  //   }
  //   
  //   // Read frame data (should be FRAME_SIZE bytes = 1024 for 128x64 display)
  //   size_t bytesRead = f.read(frame, FRAME_SIZE);
  //   f.close();
  //   
  //   if (bytesRead != FRAME_SIZE) {
  //     Serial.printf("⚠️ Frame %d incomplete: read %d bytes, expected %d\n", i, bytesRead, FRAME_SIZE);
  //     continue;
  //   }
  //   
  //   // Render frame
  //   display.clearDisplay();
  //   display.drawBitmap(0, 0, frame, SCREEN_WIDTH, SCREEN_HEIGHT, 1);
  //   display.display();
  //   
  //   delay(frameDelayMs);
  //   checkForUserActivity();
  // }
  
  // Stub for simulation
  Serial.printf("⚠️ Animation disabled: %s (SPIFFS not available in simulation)\n", folderName);
}

// Configure PIR motion sensor pin and reset timers
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
  // === DISABLED for Wokwi simulation ===
  // if (totalAnimations == 0) return;
  // srand(millis());
  // int choice = rand() % totalAnimations;
  // const AnimationFrame& anim = idleAnimations[choice];
  // if (anim.frameCount > 0) {
  //   playAnimationFromFrames(anim.name, anim.frameCount, anim.delayMs);
  // }
  Serial.println("⚠️ Random animation disabled (SPIFFS not available)");
}

void playScheduledAnimation() {
  checkForUserActivity();
  if (!inIdleAnimation || shaking) return;
  if (!isIdlePlaying) {
    Serial.println("💤 Entering idle animation loop (animations disabled in simulation)");
    isIdlePlaying = true;
  }
  // === DISABLED for Wokwi simulation ===
  // int animIndex = random(0, totalAnimations);
  // const AnimationFrame& anim = idleAnimations[animIndex];
  // if (anim.frameCount > 0) {
  //   Serial.printf("🎞 Playing idle animation: %s\n", anim.name);
  //   playAnimationFromFrames(anim.name, anim.frameCount, anim.delayMs);
  //   Serial.println("✅ Animation cycle done");
  // }
  
  // Just display a blank screen in simulation
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 28);
  display.println("Idle Mode");
  display.display();
  delay(100);
  
  isIdlePlaying = false;
}
