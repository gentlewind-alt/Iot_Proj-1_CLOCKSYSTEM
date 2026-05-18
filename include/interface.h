#ifndef INTERFACE_H
#define INTERFACE_H

#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <time.h>


#include "clock.h"
#include "sensors.h"
#include "hcsr04.h"
#include "mpu6050_angle.h"
#include "pir_motion.h"
#include "distance_widget.h"
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

extern volatile int counter;
extern std::string weatherRegion;
extern bool menuselecting;
extern bool running;
extern bool menuChoice;
extern bool rotaryUsed; 

extern bool alarmEditing; 
struct InputState {
    int rotaryDirection; // -1, 0, or 1
    bool swPressed;
    bool rstPressed;
};
extern InputState readUserInput(); 

// Interface core
void interfaceLoop(const InputState &input);
// Actions
void toggleAlarm();
void changeWeatherRegion();

// Callbacks implemented in main.cpp
void setStatusColor(uint8_t r, uint8_t g, uint8_t b);
void displayText(int x, int y, const std::string &text);
int getCounter(); 
void handleEncoderInput();

enum MenuOption {
  MENU_SELECTOR = -1,
  MENU_CLOCK,
  MENU_ALARM,
  MENU_STOPWATCH,
  MENU_DISTANCE,
  MENU_ANGLE,
  MENU_COUNT
};

extern int currentMenu;
extern Adafruit_MPU6050 mpu;

// === NEW SENSOR OBJECTS ===
extern DistanceSensor* distanceSensor;
extern LevelSensor* levelSensor;
extern MotionDetector* motionDetector;
extern DistanceWidget* distanceWidget;
extern AngleWidget* angleWidget;
#endif
