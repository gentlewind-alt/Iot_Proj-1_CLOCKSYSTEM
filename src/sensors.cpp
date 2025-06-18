#include "interface.h"

bool pirState = false;

unsigned long lastMotionTime = 0;
bool inIdleAnimation = false;
const unsigned long idleTimeout = 1 * 60 * 1000; // 1 minute
const float motionThreshold = 5;
bool idleBlockedForAlarm = false;
float smoothX = 0;
float smoothY = 0;

bool shaking = false;
unsigned long shakeStart = 0;

const unsigned long shakeDuration = 1000; // ms to show dizzy animation

Animation* currentAnim = nullptr;
int currentFrame = 0;

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
    delay(10); // Small delay between samples
  }

  // Sort and take the median for best noise rejection
  for (int i = 0; i < samples-1; i++) {
    for (int j = i+1; j < samples; j++) {
      if (readings[j] < readings[i]) {
        long t = readings[i]; readings[i] = readings[j]; readings[j] = t;
      }
    }
  }
  return readings[samples/2]; // Median value
}

void drawBitmapAt(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color) {
  for (int16_t i = 0; i < w; i++) {
    for (int16_t j = 0; j < h; j++) {
      if (pgm_read_byte(bitmap + (j * (w / 8)) + (i / 8)) & (128 >> (i & 7))) {
        if (x + i >= 0 && x + i < 128 && y + j >= 0 && y + j < 64) {
          display.drawPixel(x + i, y + j, color);
        }
      }
    }
  }
}

void motionSensorLoop() {
  bool pirVal = digitalRead(PIR_PIN);
 long distance = getDistanceCM();
  Serial.print("PIR State: ");
  Serial.print(pirVal);
  Serial.print(" | Distance: ");
  Serial.println(distance);

    if (alarmBeeping && pirVal && distance > 0 && distance < 10) {
  long distance = getDistanceCM();
  if (distance > 0 && distance < 10) {
      Serial.println("😴 Snooze triggered by motion <10cm!");
      alarmBeeping = false;
      snoozeUntil = millis() + SNOOZE_DURATION;
      // Optionally, give user feedback (e.g., beep or display message)
    }
  }
  // Rotary encoder button handling
  if (
      digitalRead(pinSW) ==  LOW || // Rotary encoder switch as OK
      digitalRead(pinRST) == LOW || // RST_PIN as menu select
      running == true   // Stopwatch running state
  ) {
    lastMotionTime = millis();
    if (inIdleAnimation) {
      Serial.println("✨ Exiting idle mode due to rotary encoder press.");
      inIdleAnimation = false;
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
      Serial.println("✨ Exiting idle mode due to motion.");
      inIdleAnimation = false;
      display.clearDisplay();
      display.display();
    }
  } else {
    if (pirState && distance > 30 && distance < 500) {
      Serial.println("🛑 Motion ended!");
      pirState = false;
    }
    if (!inIdleAnimation && (millis() - lastMotionTime > idleTimeout)) {
      Serial.println("💤 No motion or rotary for 1 min. Entering idle animation.");
      inIdleAnimation = true;
    }
  }

  if (inIdleAnimation && !shaking) { // Only run idle animation if not shaking
    if (currentAnim == nullptr) {
      int index = random(0, totalAnimations);
      currentAnim = &allAnimations[index];
      currentFrame = 0;
      Serial.print("🎞️ Starting animation #");
      Serial.println(index);
    }

    if (!currentAnim || !currentAnim->frames) {
      inIdleAnimation = false;
      currentAnim = nullptr;
      currentFrame = 0;
      return;
    }

    // --- Fix: Check bounds before drawing ---
    if (currentFrame < currentAnim->frameCount) {
      display.clearDisplay();
      orienConfig(); // Call orientation config to handle pitch/roll
      display.display();
      currentFrame += 1; // 2x speed
    } else {
      currentFrame = 0;
      currentAnim = nullptr;
    }

    delay(50); // 2x speed

  } else if (!inIdleAnimation) {
    currentAnim = nullptr; // reset animation when exiting idle
  }
}

void orienConfig() {
    sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Calculate pitch and roll
  float pitch = atan2(a.acceleration.y, a.acceleration.z) * 180.0 / PI;
  float roll = atan2(-a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180.0 / PI;

  // Smooth pitch/roll
  int16_t xOffset = abs(pitch) < 5 ? 0 : map(pitch, 90, -90, -30, 30);
  int16_t yOffset = abs(roll) < 5 ? 0 : map(roll, -45, 45, -16, 16);

  const float alpha = 0.5;
  smoothX = (1 - alpha) * smoothX + alpha * xOffset;
  smoothY = (1 - alpha) * smoothY + alpha * yOffset;

  // Shake detection
  float accMag = sqrt(a.acceleration.x * a.acceleration.x +
                      a.acceleration.y * a.acceleration.y +
                      a.acceleration.z * a.acceleration.z);
  float delta = abs(accMag - 9.81);

  // OLED rendering
  display.clearDisplay();

  static int cryFrame = 0;
  static unsigned long lastcryFrameTime = 0;
  static unsigned long dizzyStartTime = 0;

  if (delta > motionThreshold && !shaking) {
      shaking = true;
      shakeStart = millis();
      dizzyStartTime = shakeStart; // Start dizzy timer
      inIdleAnimation = false; // Stop idle animation when shaking
  }

  if (shaking) {
      // Show dizzy animation frames in a loop for up to 10 seconds or until pitch/roll settle
      unsigned long now = millis();
      if (now - lastcryFrameTime > 80) { // Change frame every 80ms
          cryFrame = (cryFrame + 1) % cry_allArray_LEN;
          lastcryFrameTime = now;
      }
      display.clearDisplay();
      display.drawBitmap(0, 0, cry_allArray[cryFrame], 128, 64, SSD1306_WHITE);
      display.display();

      // End shaking after 10 seconds or if both pitch and roll are between -20 and 20
      if ((now - dizzyStartTime > 10000) ||
          (abs(pitch) < 20 && abs(roll) < 20)) {
          shaking = false;
          cryFrame = 0;
      }
      return; // Do not run idle animation or orientation animation
  }

   if (!currentAnim || !currentAnim->frames || currentFrame < 0 || currentFrame >= currentAnim->frameCount) {
      return;
  }
  
  // Draw bitmap with smoothed offset
  int16_t x = (128 - 128) / 2 + (int16_t)smoothX;
  int16_t y = (64 - 64) / 2 + (int16_t)smoothY;
  return display.drawBitmap(x, y, currentAnim->frames[currentFrame], 128, 64, SSD1306_WHITE);
}