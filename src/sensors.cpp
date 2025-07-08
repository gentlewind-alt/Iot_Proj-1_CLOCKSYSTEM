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

const Animation* currentAnim = nullptr;
int currentFrame = 0;

unsigned long animStartTime = 0;

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

// Define animation pools
const Animation* dayAnims[] = { &allAnimations[1], &allAnimations[5], &allAnimations[6], &allAnimations[7],  &allAnimations[8], &allAnimations[9]};    // Happy, Curious, Normal
const Animation* nightAnims[] = { &allAnimations[0], &allAnimations[2], &allAnimations[3], &allAnimations[4], &allAnimations[5] };  // Sleepy, Sad, Cool
const Animation* sleepAnim = &allAnimations[0]; // Make sure index 0 is the sleeping face

AnimationSchedule schedules[] = {
  {8, 20, dayAnims, sizeof(dayAnims) / sizeof(dayAnims[0])},    // 8 AM to 8 PM
  {20, 8, nightAnims, sizeof(nightAnims) / sizeof(nightAnims[0])} // 8 PM to 8 AM (wraps midnight)
};
const int NUM_SCHEDULES = sizeof(schedules) / sizeof(schedules[0]);

const AnimationSchedule* getCurrentSchedule(int hour, int minute) {
    int nowMins = hour * 60 + minute;
    for (int i = 0; i < NUM_SCHEDULES; i++) {
        int start = schedules[i].startHour;
        int end = schedules[i].endHour;
        int startMins = start * 60;
        int endMins = end * 60;
        if (startMins < endMins) {
            if (nowMins >= startMins && nowMins < endMins) {
                Serial.print("Matched slot: "); Serial.print(start); Serial.print(" to "); Serial.println(end);
                return &schedules[i];
            }
        } else {
            // Overnight slot
            if (nowMins >= startMins || nowMins < endMins) {
                Serial.print("Matched slot: "); Serial.print(start); Serial.print(" to "); Serial.println(end);
                return &schedules[i];
            }
        }
    }
    Serial.println("No slot matched!");
    return nullptr;
}

void motionSensorLoop() {
  bool pirVal = digitalRead(PIR_PIN);
  long distance = getDistanceCM();

  // Serial.print("PIR State: ");
  // Serial.print(pirVal);
  // Serial.print(" | Distance: ");
  // Serial.println(distance);

  // Snooze on motion near sensor
  if (alarmBeeping && pirVal && distance > 0 && distance < 10) {
    Serial.println("😴 Snooze triggered by motion <10cm!");
    alarmBeeping = false;
    snoozeUntil = millis() + SNOOZE_DURATION;
  }

  // Rotary encoder/button or stopwatch running disables idle animation
  if (digitalRead(pinCLK) != 0 ||
      digitalRead(pinDT) != 0 ||
      digitalRead(pinSW) == LOW ||
      digitalRead(pinRST) == LOW ||
      running == true
  ) {
    lastMotionTime = millis();
    if (inIdleAnimation) {
      Serial.println("✨ Exiting idle mode due to rotary encoder press.");
      inIdleAnimation = false;
      animStartTime = 0;
      display.clearDisplay();
      display.display();
    }
  }

  // PIR motion disables idle animation
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

  // === Idle Animation Handling ===
  if (inIdleAnimation && !shaking) {
    if (currentAnim == nullptr || (millis() - animStartTime > 30000)) { // 30 seconds
      DateTime now = rtc.now();
      const TimeZone& tz = timeZones[selectedTimeZoneIndex];
      DateTime localNow = now + TimeSpan((int)(tz.offsetHours * 3600));
      const AnimationSchedule* schedule = getCurrentSchedule(localNow.hour(), localNow.minute());


      if (schedule && schedule->numAnims > 0) {
        bool isNight = (schedule->startHour == 20); // 8PM start
        int index = 0;

        if (isNight && random(0, 100) < 70) {
          // 70% chance to pick sleep animation at night
          currentAnim = sleepAnim;
        } else {
          // Pick random from pool (could include sleep too)
          index = random(0, schedule->numAnims);
          currentAnim = schedule->anims[index];
        }

        currentFrame = 0;
        animStartTime = millis();

        Serial.print("🎞️ Playing animation ");
        if (currentAnim == sleepAnim && millis() - animStartTime < 1*60*60000) {
          Serial.println("🛌 Sleep (priority night)");
        } else {
          Serial.print("Index ");
          Serial.print(index);
          Serial.print(" from ");
          Serial.print(schedule->startHour);
          Serial.print(" to ");
          Serial.println(schedule->endHour);
        }
      }
    }

    if (!currentAnim || !currentAnim->frames) {
      inIdleAnimation = false;
      currentAnim = nullptr;
      currentFrame = 0;
      return;
    }

    if (currentFrame < currentAnim->frameCount) {
      display.clearDisplay();
      orienConfig();  // This draws currentAnim[currentFrame] with pitch/roll
      display.display();
      currentFrame += 1;
    } else {
      currentFrame = 0;
      currentAnim = nullptr;
    }

    delay(50);  // Control speed
  } else if (!inIdleAnimation) {
    currentAnim = nullptr;
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