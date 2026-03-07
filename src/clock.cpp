#include "clock.h"
#include "interface.h"
#include <time.h>

// We will use getTimeWithFallback() from main.cpp
extern bool getTimeWithFallback(struct tm &timeinfo);

// — Timezone definitions —
const TimeZone timeZones[] = {
  {"IST",  +5.5}, {"UTC",   0.0}, {"PST", -13.5}, {"MST", -12.5},
  {"CST", -11.5}, {"EST", -10.5}, {"GMT",  -5.5}, {"CET",  -4.5},
  {"EET",  -3.5}, {"MSK",  -2.5}, {"CST(China)", 2.5}, {"JST",  3.5},
  {"AEST", 4.5}, {"AEDT",  5.5}, {"NZST",  6.5},
};
const int tzCount = sizeof(timeZones) / sizeof(timeZones[0]);

// — Musical chords —
float Cmaj7[] = {261.63, 329.63, 392.00, 493.88};
float G[]     = {196.00, 246.94, 293.66};
float Am7[]   = {220.00, 261.63, 329.63, 392.00};
float Fmaj7[] = {174.61, 220.00, 261.63, 329.63};

// — Alarm globals —
bool    alarmEnabled      = true;
int     alarmHour         = 18;
int     alarmMinute       = 33;
volatile bool alarmBeeping = false;
unsigned long snoozeUntil = 0;
unsigned long alarmStartTime = 0;

int selectedTimeZoneIndex = 0;

const unsigned long SNOOZE_DURATION = 5 * 60 * 1000; // 5 min
#define BUZZER_PIN     10
#define BUZZER_CHANNEL 0

// — Chord playback state —
int  alarmChordIndex    = 0;
unsigned long lastAlarmNoteTime  = 0;

// — NTP sync state —
unsigned long lastNTPSync     = 0;
const unsigned long ntpSyncInterval = 6UL * 60UL * 60UL * 1000UL; // 6h

// — Utility: play a chord on the buzzer —
void playChord(const float* chord, int len) {
  ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
  for (int i = 0; i < len; i++) {
    ledcWriteTone(BUZZER_CHANNEL, chord[i]);
    delay(333);
    ledcWriteTone(BUZZER_CHANNEL, 0);
    delay(30);
  }
  ledcDetachPin(BUZZER_PIN);
}

// — Alarm check using `struct tm` —
void checkAndTriggerAlarm(const struct tm& now) {
  static int  lastCheckedMinute       = -1;
  static bool alarmTriggeredThisMinute = false;

  if (!alarmEnabled) return;

  struct tm nowCopy = now;
  time_t epoch = mktime(&nowCopy);
  if (snoozeUntil && epoch < snoozeUntil) return;

  if (now.tm_min != lastCheckedMinute) {
    alarmTriggeredThisMinute = false;
    lastCheckedMinute = now.tm_min;
  }

  if (!alarmTriggeredThisMinute &&
      now.tm_hour == alarmHour &&
      now.tm_min  == alarmMinute) {
    Serial.println("🔔 Alarm Triggered!");
    alarmBeeping = true;
    alarmStartTime = millis();
    alarmTriggeredThisMinute = true;
  }
}

// — Draw the alarm settings UI —
void displayAlarmSettings() {
  display.clearDisplay();
  char buf[32];
  snprintf(buf, sizeof(buf), "Alarm: %s", alarmEnabled ? "ON" : "OFF");
  display.setCursor(0, 40); display.print(buf);
  snprintf(buf, sizeof(buf), "Time: %02d:%02d", alarmHour, alarmMinute);
  display.setCursor(0, 50); display.print(buf);
  display.display();
}

// — Show clock with timezone —
void showClockPage() {
  const TimeZone& tz = timeZones[selectedTimeZoneIndex];
  struct tm now;

  // ✅ Use fallback-aware time fetch
  if (!getTimeWithFallback(now)) {
    display.setCursor(0, 0);
    display.print("Time Error");
    display.display();
    return;
  }

  int offsetH = (int)tz.offsetHours;
  int offsetM = (tz.offsetHours - offsetH) * 60;  // For fractional offsets

  now.tm_hour += offsetH;
  now.tm_min  += offsetM;
  mktime(&now);  // Normalize

  // Time string
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf),
           "%02d:%02d:%02d", now.tm_hour, now.tm_min, now.tm_sec);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(tz.name);
  display.setTextSize(2);
  int16_t x, y; uint16_t w, h;
  display.getTextBounds(timeBuf, 0, 0, &x, &y, &w, &h);
  display.setCursor((128 - w)/2, 25);
  display.print(timeBuf);
  display.drawLine(0, 25 + h + 2, 127, 25 + h + 2, WHITE);

  // Date
  char dateBuf[16];
  snprintf(dateBuf, sizeof(dateBuf),
           "%02d/%02d/%04d", now.tm_mday, now.tm_mon+1, now.tm_year+1900);
  display.setTextSize(1);
  display.getTextBounds(dateBuf, 0, 0, &x, &y, &w, &h);
  display.setCursor((128 - w)/2, 25 + h + 15);
  display.print(dateBuf);
}

// — Show alarm ON/OFF on main UI —
void showAlarmStatus(const InputState& input) {
  static bool lastswPressed = false;
  static unsigned long lastAlarmToggleTime = 0;
  const unsigned long ALARM_TOGGLE_DEBOUNCE = 300; // ms
  
  // Non-blocking toggle with debounce
  if (input.swPressed && !lastswPressed && (millis() - lastAlarmToggleTime) >= ALARM_TOGGLE_DEBOUNCE) {
    alarmEnabled = !alarmEnabled;
    lastAlarmToggleTime = millis();
  }
  lastswPressed = input.swPressed;

  char alarmBuf[32];
  snprintf(alarmBuf, sizeof(alarmBuf), "| Alarm:%s %02d:%02d",
           alarmEnabled ? "ON" : "OFF", alarmHour, alarmMinute);
  display.setCursor(26, 0);
  display.print(alarmBuf);
}

// — Timezone cycling —
void changeTimeZone() {
  selectedTimeZoneIndex = (selectedTimeZoneIndex + 1) % tzCount;
}

// — Optional manual set — 
void setTimeZoneIndex(int idx) {
  if (idx >= 0 && idx < tzCount) selectedTimeZoneIndex = idx;
}

// — Stopwatch screen —
void stopWatch(const InputState& input) {
  static unsigned long startTime = 0;
  static unsigned long elapsed = 0;
  static bool lastSwPressed = false;
  static bool lastRstPressed = false;
  static unsigned long lastSwPressTime = 0;
  static unsigned long lastRstPressTime = 0;
  const unsigned long SW_DEBOUNCE = 100; // ms
  const unsigned long RST_DEBOUNCE = 100; // ms

  // Non-blocking start/stop with debounce
  if (input.swPressed && !lastSwPressed && (millis() - lastSwPressTime) >= SW_DEBOUNCE) {
    running = !running;
    if (running) {
      startTime = millis() - elapsed;
    } else {
      elapsed = millis() - startTime;
    }
    lastSwPressTime = millis();
  }
  lastSwPressed = input.swPressed;

  // Non-blocking reset with debounce
  if (input.rstPressed && !lastRstPressed && (millis() - lastRstPressTime) >= RST_DEBOUNCE) {
    startTime = 0;
    elapsed = 0;
    running = false;
    lastRstPressTime = millis();
  }
  lastRstPressed = input.rstPressed;
  display.clearDisplay();

  int boxX = 10, boxY = 12, boxW = 108, boxH = 40, radius = 8;
  display.drawRoundRect(boxX, boxY, boxW, boxH, radius, WHITE);

  int iconX = boxX + 6, iconY = boxY + 10;
  if (running) {
    display.fillRect(iconX, iconY, 4, 14, WHITE);
    display.fillRect(iconX + 8, iconY, 4, 14, WHITE);
  } else {
    display.fillTriangle(iconX, iconY, iconX, iconY + 14, iconX + 12, iconY + 7, WHITE);
  }

  unsigned long displayTime = running ? (millis() - startTime) : elapsed;
  unsigned int totalSeconds = displayTime / 1000;
  unsigned int minutes = totalSeconds / 60;
  unsigned int seconds = totalSeconds % 60;
  unsigned int tenths = (displayTime % 1000) / 100;

  char buf[16];
  snprintf(buf, sizeof(buf), "%02u:%02u:%01u", minutes, seconds, tenths);
  display.setTextSize(2);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  int textX = boxX + (boxW - w) / 2 + 8;
  int textY = boxY + (boxH - h) / 2;
  display.setCursor(textX, textY);
  display.print(buf);

  display.setTextSize(1);
  display.setCursor(34, boxY + boxH + 4);
  display.print("OK: Start/Stop");

  display.display();
}
