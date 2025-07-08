#include "clock.h"
#include "interface.h" // for getUserInput()
#include <Arduino.h>
#include <RTClib.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// List of standard time zones you want to cycle through:
const TimeZone timeZones[] = {
  {"IST", +5.5},
  {"UTC", 0.0},
  {"PST", -13.5},
  {"MST", -12.5},
  {"CST", -11.5},
  {"EST", -10.5},
  {"GMT", -5.5},
  {"CET", -4.5},
  {"EET", -3.5},
  {"MSK", -2.5},
  {"CST(China)", 2.5},
  {"JST", 3.5},
  {"AEST", 4.5},
  {"AEDT", 5.5},
  {"NZST", 6.5},
};
const int tzCount = sizeof(timeZones) / sizeof(timeZones[0]);
float Cmaj7[] = {261.63, 329.63, 392.00, 493.88};
float G[]     = {196.00, 246.94, 293.66};
float Am7[]   = {220.00, 261.63, 329.63, 392.00};
float Fmaj7[] = {174.61, 220.00, 261.63, 329.63};

int selectedTimeZoneIndex = 0;
const int noteDuration = 333; // eighth note duration
const int noteGap = 30;   

bool alarmEnabled = true;
int alarmHour = 18;
int alarmMinute = 33;

volatile bool alarmBeeping = false;
unsigned long snoozeUntil = 60 * 1000;

byte seconds = 0;
byte minutes = 0;
byte milliseconds = 0;

#define BUZZER_PIN 10
#define BUZZER_CHANNEL 0

unsigned long alarmStartTime = 0;

const unsigned long SNOOZE_DURATION = 5 * 60 * 1000; // 5 minutes in ms

void playChord(const float* chord, int len) {
  ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL); // Attach only once per session if possible
  for (int i = 0; i < len; i++) {
    ledcWriteTone(BUZZER_CHANNEL, chord[i]);
    delay(noteDuration);
    ledcWriteTone(BUZZER_CHANNEL, 0); // Stop tone
    delay(noteGap);
  }
  ledcDetachPin(BUZZER_PIN); // Optional: detach after playing
}

// Alarm functions
void checkAndTriggerAlarm(const DateTime& now) {
    static int lastCheckedMinute = -1;
    static bool alarmTriggeredThisMinute = false;

    if (!alarmEnabled) return;
    if (snoozeUntil && now.unixtime() < snoozeUntil) return;

    // Reset trigger flag when minute changes
    if (now.minute() != lastCheckedMinute) {
        alarmTriggeredThisMinute = false;
        lastCheckedMinute = now.minute();
    }

    // Trigger alarm if not already triggered this minute
    if (!alarmTriggeredThisMinute &&
        now.hour() == alarmHour &&
        now.minute() == alarmMinute) {
        Serial.println("🔔 Alarm Triggered!");
        alarmBeeping = true;
        alarmStartTime = millis();
        alarmTriggeredThisMinute = true;
        inIdleAnimation = false; // Stop idle animation if alarm is triggered
         // Stop idle animation if alarm is triggered
    }
}

void displayAlarmSettings() {
  display.clearDisplay();

  char buf[32];
  snprintf(buf, sizeof(buf), "Alarm: %s", alarmEnabled ? "ON" : "OFF");
  display.setCursor(0, 40);
  display.print(buf);

  snprintf(buf, sizeof(buf), "Time: %02d:%02d", alarmHour, alarmMinute);
  display.setCursor(0, 50);
  display.print(buf);

  display.display();
}

// --- Clock Display Functions ---
// Update displayed time adjusted by current timezone
void showClockPage(const DateTime& nowUtc) {
    const TimeZone& tz = timeZones[selectedTimeZoneIndex];
    DateTime adjustedTime = nowUtc + TimeSpan((int)(tz.offsetHours * 3600));

    // Show timezone name
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("TZ:");
    display.print(tz.name);
    display.setTextSize(1);
    
    // Show time in large font, centered
    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", adjustedTime.hour(), adjustedTime.minute(), adjustedTime.second());
    display.setTextSize(2);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(timeBuf, 0, 0, &x1, &y1, &w, &h);
    int textX = (128 - w) / 2;
    int textY = 25;
    display.setCursor(textX, textY);
    display.print(timeBuf);

    // Draw a thin line below the time
    display.drawLine(0, textY + h + 2, 127, textY + h + 2, WHITE);

    // Show date below the line, small font, centered
    display.setTextSize(1);
    char dateBuf[17];
    snprintf(dateBuf, sizeof(dateBuf), "%02d/%02d/%04d", adjustedTime.day(), adjustedTime.month(), adjustedTime.year());
    display.getTextBounds(dateBuf, 0, 0, &x1, &y1, &w, &h);
    int dateX = (128 - w) / 2;
    int dateY = textY + h + 15; // 18px below the time
    display.setCursor(dateX, dateY);
    display.print(dateBuf);
}

void showAlarmStatus() {
  static bool lastswPressed = false;
  // Toggle alarm status if OK button is pressed
  InputState input = readUserInput();
  // int lastRotaryDirection = input.rotaryDirection;
  if (input.swPressed && !lastswPressed) { // Assuming 1 is OK button
    alarmEnabled = !alarmEnabled;
    delay(300); // Debounce
  }
  lastswPressed = input.swPressed;
  // lastRotaryDirection = input.rotaryDirection;
  // Show alarm status and time at the bottom
  char alarmBuf[32];
  snprintf(alarmBuf, sizeof(alarmBuf), "|Alarm:%s %02d:%02d", alarmEnabled ? "ON" : "OFF", alarmHour, alarmMinute);
  display.setCursor(38, 0);
  display.print(alarmBuf);
}

// Cycle to next time zone in list
void changeTimeZone() {
    selectedTimeZoneIndex = (selectedTimeZoneIndex + 1) % tzCount;
}

// Optionally, a function to set timezone index manually:
void setTimeZoneIndex(int index) {
  if (index >= 0 && index < tzCount) {
    selectedTimeZoneIndex = index;
  }
}

void syncRTCWithNTP() {
  configTime(0, 0, "pool.ntp.org"); // Use UTC; adjust offset if needed
  struct tm timeinfo;
  const int maxRetries = 5;
  int attempt = 0;
  bool success = false;

  while (attempt < maxRetries) {
    if (getLocalTime(&timeinfo)) {
      rtc.adjust(DateTime(
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec
      ));
      Serial.println("✅ RTC synced with NTP!");
      success = true;
      break;
    } else {
      Serial.println("❌ Failed to get time from NTP, retrying...");
      delay(1000); // Wait 1 second before retrying
      attempt++;
    }
  }

  if (!success) {
    Serial.println("❌ Failed to sync RTC with NTP after retries.");
  }
}

void stopWatch() {
  static unsigned long startTime = 0;
  static unsigned long elapsed = 0;
  static int la = 0;
  counter = 0;

  InputState input = readUserInput();
  if (input.swPressed) { // OK button to start/stop
    running = !running;
    if (running) {
      startTime = millis() - elapsed; // Continue from where left off
    } else {
      elapsed = millis() - startTime;
    }
    delay(100); // Debounce
  }

  if (input.rstPressed) {
    startTime = 0;
    elapsed = 0;
    running = false; // Debugging line to show counter value
  }
  display.clearDisplay();

  // Draw rounded rectangle box for stopwatch area
  int boxX = 10, boxY = 12, boxW = 108, boxH = 40, radius = 8;
  display.drawRoundRect(boxX, boxY, boxW, boxH, radius, WHITE);

  // Draw play/pause icon at left inside the box
  int iconX = boxX + 6, iconY = boxY + 10;
  if (running) {
    // Draw "pause" icon (two vertical bars)
    display.fillRect(iconX, iconY, 4, 14, WHITE);
    display.fillRect(iconX + 8, iconY, 4, 14, WHITE);
  } else {
    // Draw "play" icon (triangle)
    display.fillTriangle(iconX, iconY, iconX, iconY + 14, iconX + 12, iconY + 7, WHITE);
  }

  // Draw stopwatch time or "Stopped" centered in the box
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
  int textX = boxX + (boxW - w) / 2 + 8; // +8 to offset for icon
  int textY = boxY + (boxH - h) / 2;
  display.setCursor(textX, textY);
  display.print(buf);

  // Draw label below the box
  display.setTextSize(1);
  display.setCursor(34, boxY + boxH + 4);
  display.print("OK: Start/Stop");

  display.display();
}