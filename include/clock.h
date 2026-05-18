#ifndef CLOCK_H
#define CLOCK_H

#include <Arduino.h>
#include <RTClib.h>
#include <Adafruit_SSD1306.h>

// Your display instance (extern, declared elsewhere)
extern Adafruit_SSD1306 display;
extern volatile bool alarmBeeping;
extern const unsigned long SNOOZE_DURATION; // Snooze duration in milliseconds
extern unsigned long snoozeUntil; // Snooze end time
extern unsigned long alarmStartTime;
extern int alarmHour;      // Alarm hour
extern int alarmMinute;    // Alarm minute
extern bool alarmEnabled;  // Alarm enabled state
extern bool running;       // Stopwatch running state
extern float Cmaj7[];
extern float G[];
extern float Am7[];
extern float Fmaj7[];
extern int alarmChordIndex; // Current chord index
extern unsigned long lastAlarmNoteTime; // Last time a note was played
extern unsigned long lastNTPSync;
// Forward declare InputState
struct InputState;

// Function declarations
void stopWatch(const InputState& input);
void showAlarmStatus(const InputState& input);
bool syncRTCWithNTP();
// Modular display functions
void playChord(const float* chord, int len);
void checkAndTriggerAlarm(const tm& now);
void showClockPage();
 // Dummy function to avoid undefined identifier error

#endif
