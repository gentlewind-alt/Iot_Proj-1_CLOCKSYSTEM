#include "animation.h"
#include "sensors.h"

#include <AnimatedGIF.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#define READ_EMOTE(array, index, out) memcpy_P(&(out), &(array)[index], sizeof(Emote))

extern const uint8_t _1[] PROGMEM;
extern const uint8_t _2[] PROGMEM;
extern const uint8_t _3[] PROGMEM;
extern const uint8_t _4[] PROGMEM;
extern const uint8_t _5[] PROGMEM;
extern const uint8_t _6[] PROGMEM;
extern const uint8_t _8[] PROGMEM;
extern const uint8_t _9[] PROGMEM;
extern const uint8_t _10[] PROGMEM;
extern const uint8_t _14[] PROGMEM;
extern const uint8_t _16[] PROGMEM;
extern const uint8_t _21[] PROGMEM;
extern const uint8_t _22[] PROGMEM;
extern const uint8_t _23[] PROGMEM;
extern const uint8_t _24[] PROGMEM;
extern const uint8_t _25[] PROGMEM;
extern const uint8_t _28[] PROGMEM;
extern const uint8_t _29[] PROGMEM;
extern const uint8_t _31[] PROGMEM;
extern const uint8_t _32[] PROGMEM;
extern const uint8_t _33[] PROGMEM;
extern const uint8_t _34[] PROGMEM;
extern const uint8_t _35[] PROGMEM;
extern const uint8_t _36[] PROGMEM;
extern const uint8_t _37[] PROGMEM;
extern const uint8_t _38[] PROGMEM;
extern const uint8_t _39[] PROGMEM;
extern const uint8_t _41[] PROGMEM;
extern const uint8_t jojos[] PROGMEM;


const size_t _1_len = sizeof(_1);
const size_t _2_len = sizeof(_2);
const size_t _3_len = sizeof(_3);
const size_t _4_len = sizeof(_4);
const size_t _5_len = sizeof(_5);
const size_t _6_len = sizeof(_6);
const size_t _8_len = sizeof(_8);
const size_t _9_len = sizeof(_9);
const size_t _10_len = sizeof(_10);
const size_t _14_len = sizeof(_14);
const size_t _16_len = sizeof(_16);
const size_t _21_len = sizeof(_21);
const size_t _22_len = sizeof(_22);
const size_t _23_len = sizeof(_23);
const size_t _24_len = sizeof(_24);
const size_t _25_len = sizeof(_25);
const size_t _28_len = sizeof(_28);
const size_t _29_len = sizeof(_29);
const size_t _32_len = sizeof(_32);
const size_t _33_len = sizeof(_33);
const size_t _34_len = sizeof(_34);
const size_t _35_len = sizeof(_35);
const size_t _36_len = sizeof(_36);
const size_t _37_len = sizeof(_37);
const size_t _38_len = sizeof(_38);
const size_t _41_len = sizeof(_41);

#include <map>
static std::map<const Emote*, int> scheduleProgress;
extern Adafruit_SSD1306 display;
extern Adafruit_MPU6050 mpu;
const unsigned long idleTimeout = 20 * 1000;
float smoothX = 0, smoothY = 0;
unsigned long shakeStart = 0;
bool shaking = false;
const float motionThreshold = 2.5;
bool pirState = false;
unsigned long lastMotionTime = 0;
bool inIdleAnimation = false;
bool isIdlePlaying = false; 
int progressIndex = 0;
const AnimationSchedule* activeSchedule = nullptr;
unsigned long lastInteractionTime = 0;

const Emote firsthalf_Anims[] PROGMEM= {
     {_1, sizeof(_1)}, {_2, sizeof(_2)}, {_3, sizeof(_3)}, 
  {_4, sizeof(_4)}, {_5, sizeof(_5)},
  {_6, sizeof(_6)}, {_8, sizeof(_8)}, {_9, sizeof(_9)}, {_10, sizeof(_10)}, {_16, sizeof(_16)},
  {_21, sizeof(_21)}, {_22, sizeof(_22)}, {_23, sizeof(_23)}, {_24, sizeof(_24)}, {_25, sizeof(_25)},
  {_28, sizeof(_28)}, {_29, sizeof(_29)}, {_32, sizeof(_32)}, {_33, sizeof(_33)},
  {_34, sizeof(_34)}, {_35, sizeof(_35)}, {_36, sizeof(_36)}, {_37, sizeof(_37)}, {_38, sizeof(_38)},
  {_41, sizeof(_41)}
};

const Emote secondhalf_Anims[] PROGMEM = {
    {_14, sizeof(_14)}, {_3, sizeof(_3)}, {_4, sizeof(_4)}, {_22, sizeof(_22)}, {_37, sizeof(_37)}
};

const AnimationSchedule schedules[] = {
  {8, 20, firsthalf_Anims, sizeof(firsthalf_Anims) / sizeof(firsthalf_Anims[0])},
  {20, 8, secondhalf_Anims, sizeof(secondhalf_Anims) / sizeof(secondhalf_Anims[0])}
};

const int NUM_SCHEDULES = sizeof(schedules) / sizeof(schedules[0]);

const AnimationSchedule* getCurrentSchedule() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("❌ Failed to get local time");
    return nullptr;
  }

  int nowMins = timeinfo.tm_hour * 60 + timeinfo.tm_min;

  for (int i = 0; i < NUM_SCHEDULES; i++) {
    int startMins = schedules[i].startHour * 60;
    int endMins = schedules[i].endHour * 60;

    if (startMins < endMins) {
      // Regular daytime slot
      if (nowMins >= startMins && nowMins < endMins) {
        Serial.printf("⏰ Matched slot: %d to %d\n", schedules[i].startHour, schedules[i].endHour);
        return &schedules[i];
      }
    } else {
      // Overnight slot (e.g. 20:00 – 08:00)
      if (nowMins >= startMins || nowMins < endMins) {
        Serial.printf("🌙 Matched overnight slot: %d to %d\n", schedules[i].startHour, schedules[i].endHour);
        return &schedules[i];
      }
    }
  }

  Serial.println("⚠️ No matching schedule found.");
  return nullptr;
}

bool isWhite(uint16_t color) {
  uint8_t r = (color >> 11) & 0x1F;
  uint8_t g = (color >> 5) & 0x3F;
  uint8_t b = color & 0x1F;
  int brightness = r * 3 + g * 6 + b;
  return brightness > 400;
}

void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *pixels = pDraw->pPixels;
  uint16_t *palette = pDraw->pPalette;
  int width = pDraw->iWidth;

  // for (int x = 0; x < width; x++) {
  //   if (pDraw->ucHasTransparency && pixels[x] == pDraw->ucTransparent) continue;
    
  //   uint16_t color = palette[pixels[x]];
  //   bool white = isWhite(color);
  //   int16_t drawX = pDraw->iX + x + round(smoothX);
  //   int16_t drawY = pDraw->iY + pDraw->y + round(smoothY);
  //   if (drawX >= 0 && drawX < 128 && drawY >= 0 && drawY < 64) {
  //     display.drawPixel(drawX, drawY, white ? SSD1306_WHITE : SSD1306_BLACK);
  //   }
  // }
  for (int x = 0; x < width; x++) {
    if (pDraw->ucHasTransparency && pixels[x] == pDraw->ucTransparent) continue;
    uint16_t color = palette[pixels[x]];
    bool white = isWhite(color);
    display.drawPixel(pDraw->iX + x, pDraw->iY + pDraw->y, white ? SSD1306_WHITE : SSD1306_BLACK);
  }

  if (pDraw->y == pDraw->iHeight - 1) {
    display.display();
  }
  // // while(!shaking ){
  //   if (pDraw->y == 0) {
  //   display.clearDisplay();  // Clears at the top of each frame render
  //   delay(100);  // Small delay to ensure display is ready
  //   }
  // // }
}

void playGIFFrame(const uint8_t* data, size_t size) {
  uint8_t* ramCopy = (uint8_t*)malloc(size);
  if (!ramCopy) {
    Serial.println("❌ Failed to allocate RAM for GIF");
    return;
  }

  for (size_t i = 0; i < size; i++) {
    ramCopy[i] = pgm_read_byte(&data[i]);
  }

  gif.begin(LITTLE_ENDIAN_PIXELS);
  if (gif.open(ramCopy, size, GIFDraw)) {
    while (gif.playFrame(true, NULL)) {
      display.clearDisplay();  // 💡 Clear once per full frame, not per line
      // Frame is rendered via GIFDraw()
      display.display();
    }
    gif.close();
  } else {
    Serial.println("❌ Failed to open GIF from RAM copy");
  }

  free(ramCopy);  // Always free after use
}


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

  for (int i = 0; i < samples-1; i++) {
    for (int j = i+1; j < samples; j++) {
      if (readings[j] < readings[i]) {
        long t = readings[i]; readings[i] = readings[j]; readings[j] = t;
      }
    }
  }
  return readings[samples/2];
}

// void orienConfig() {
//   sensors_event_t a, g, temp;
//   mpu.getEvent(&a, &g, &temp);

//   float pitch = atan2(a.acceleration.y, a.acceleration.z) * 180.0 / PI;
//   float roll = atan2(-a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180.0 / PI;

//   int16_t xOffset = abs(pitch) < 5 ? 0 : map(pitch, 90, -90, -30, 30);
//   int16_t yOffset = abs(roll) < 5 ? 0 : map(roll, -45, 45, -16, 16);

//   const float alpha = 0.5;
//   smoothX = (1 - alpha) * smoothX + alpha * xOffset;
//   smoothY = (1 - alpha) * smoothY + alpha * yOffset;

//   float accMag = sqrt(a.acceleration.x * a.acceleration.x +
//                       a.acceleration.y * a.acceleration.y +
//                       a.acceleration.z * a.acceleration.z);
//   float delta = abs(accMag - 9.81);

//   Serial.printf("Pitch: %.2f°, Roll: %.2f°\n", pitch, roll);

//   static unsigned long lastReactionTime = 0;
//   unsigned long now = millis();

//   // 👁️ Left/Right Look Triggers (run only once per second)
//   if (now - lastReactionTime > 1000) {
//     if (pitch <= -60 && pitch >= -90) {
//       Serial.println("👉 Detected 'look right'");
//       display.clearDisplay();
//       playGIFFrame(_3, sizeof(_3));  // Replace _23 with your right-look GIF
//       lastReactionTime = now;
//       return;
//     } else if (pitch >= 60 && pitch <= 90) {
//       Serial.println("👈 Detected 'look left'");
//       display.clearDisplay();
//       playGIFFrame(_4, sizeof(_4));  // Replace _24 with your left-look GIF
//       lastReactionTime = now;
//       return;
//     }
//   }

//   // Shaking Logic (if no extreme pitch)
//   static unsigned long lastcryFrameTime = 0;
//   static unsigned long dizzyStartTime = 0;

//   // if (delta > motionThreshold && !shaking) {
//   //   shaking = true;
//   //   shakeStart = millis();
//   //   dizzyStartTime = shakeStart;
//   // }

//   if (shaking) {
//     if (now - lastcryFrameTime > 80) {
//       lastcryFrameTime = now;
//     }
//     display.clearDisplay();
//     playGIFFrame(_36, sizeof(_36));

//     if ((now - dizzyStartTime > 10000) || (abs(pitch) < 20 && abs(roll) < 20)) {
//       shaking = false;
//     }
//   }
// }

void checkForUserActivity() {
  bool pirVal = digitalRead(PIR_PIN);
  long distance = getDistanceCM();

  bool buttonUsed = digitalRead(pinSW) == LOW || digitalRead(pinRST) == LOW 
  // || rotaryUsed == true
  ;

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

  // If no activity for 60s, enter idle
  if (!inIdleAnimation && (millis() - lastMotionTime > idleTimeout)) {
    Serial.println("💤 Timeout passed. Entering idle mode.");
    inIdleAnimation = true;
    isIdlePlaying = false;  // Reset sequence when re-entering
  }
}

void playScheduledAnimation() {
  checkForUserActivity();  // Check for PIR/Ultrasonic motion 
  if (!inIdleAnimation || shaking) return;
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  int hour = timeinfo->tm_hour;
  int minute = timeinfo->tm_min;

  const AnimationSchedule* currentSchedule = getCurrentSchedule();

  if (!currentSchedule || currentSchedule->count == 0) {
    Serial.println("⚠️ No valid animation schedule found.");
    return;
  }

  // If idle just started or schedule changed
  if (!isIdlePlaying || currentSchedule != activeSchedule) {
    if (!isIdlePlaying) {
      Serial.println("💤 Entering idle animation loop");
    } else {
      Serial.print("🔄 Schedule changed. New slot: ");
      Serial.print(currentSchedule->startHour);
      Serial.print(" to ");
      Serial.println(currentSchedule->endHour);
    }

    isIdlePlaying = true;
    inIdleAnimation = true;
    activeSchedule = currentSchedule;
    progressIndex = 0;
  }

  if (progressIndex >= activeSchedule->count) {
    Serial.println("✅ Idle animation cycle complete.");
    isIdlePlaying = false;
    inIdleAnimation = false;
    lastInteractionTime = millis();  // Prevent instant replay
    return;
  }

  int index = random(0, activeSchedule->count);
  const Emote& selectedEmote = activeSchedule->anims[index];

  gif.begin(LITTLE_ENDIAN_PIXELS);
  if (gif.open((uint8_t*)selectedEmote.data, selectedEmote.size, GIFDraw)) {
    Serial.printf("🎞️ Playing animation %d/%d\n", progressIndex + 1, activeSchedule->count);
    while (gif.playFrame(true, NULL)) {
      // orienConfig(); 
      display.display();
      delay(1);
    }
    gif.close();
  } else {
    Serial.println("❌ Failed to open GIF.");
  }

  progressIndex++;
}