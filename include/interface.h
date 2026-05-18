#ifndef INTERFACE_H
#define INTERFACE_H

#include <Adafruit_SSD1306.h>
#include <Wire.h>
// #include <WiFi.h>  // ⚠️ DISABLED: WiFi BSS (935 KB) exceeds ESP32-C3 RAM (320 KB)
#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <time.h>


#include "clock.h"
#include "sensors.h"
#include "hcsr04.h"
#include "mpu6050_angle.h"
#include "pir_motion.h"
#include "distance_widget.h"  // Include widget classes for use in interface.cpp
#include "angle_widget.h"

#define I2C_SDA         5
#define I2C_SCL         6
#define NEOPIXEL_PIN    8
#define BUZZER_PIN      10
#define PIR_PIN         7
#define pinRST          1
#define TRIG_PIN        18
#define ECHO_PIN        19
#define pinCLK          2
#define pinDT           3
#define pinSW           4 

extern int timeZoneOffset;
extern volatile int counter;
extern std::string weatherRegion;
extern bool menuselecting;
extern bool running;
extern bool menuChoice;
extern bool rotaryUsed; // Flag to indicate if rotary was used

extern bool alarmEditing; // Flag to indicate if alarm is being edited
 // Reads rotary encoder and button states
struct InputState {
    int rotaryDirection; // -1, 0, or 1
    bool swPressed;
    bool rstPressed;
};
extern InputState readUserInput(); // Reads rotary encoder and button states

// Interface core
void interfaceLoop(const InputState &input);
// Actions
void toggleAlarm();
void changeTimeZone();
void changeWeatherRegion();
// Callbacks implemented in main.cpp
void setStatusColor(uint8_t r, uint8_t g, uint8_t b);
void displayText(int x, int y, const std::string &text);
int getCounter(); // 1=OK, 2=Next, 3=Prev
void handleEncoderInput();

enum MenuOption {
  MENU_SELECTOR = -1,
  MENU_CLOCK,
  MENU_ALARM,
  MENU_STOPWATCH,
  MENU_DISTANCE,      // NEW: Distance sensor widget
  MENU_ANGLE,         // NEW: Angle/level sensor widget
  MENU_COUNT
}; // === Menu Options ===

constexpr int MENU_COUNT_CONST = static_cast<int>(MENU_COUNT);

extern int currentMenu;
extern Adafruit_MPU6050 mpu;

// === NEW SENSOR OBJECTS (Phase 2) ===
extern DistanceSensor* distanceSensor;
extern LevelSensor* levelSensor;
extern MotionDetector* motionDetector;
extern DistanceWidget* distanceWidget;
extern AngleWidget* angleWidget;
#endif