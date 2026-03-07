#ifndef INTERFACE_H
#define INTERFACE_H

#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <WiFi.h>
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "MPU6050.h"
#include "I2Cdev.h"
#include <time.h>


#include "clock.h"
#include "weather.h"


#define I2C_SDA         8
#define I2C_SCL         9
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
extern bool choice;
extern bool rotaryUsed; // Flag to indicate if rotary was used
extern bool alarmEditing; // Flag to indicate if alarm is being edited
 // Reads rotary encoder and button states
struct InputState {
    int rotaryDirection; // -1, 0, or 1
    bool swPressed;
    bool rstPressed;
};
extern InputState readUserInput(); // Reads rotary encoder and button states
#include "sensors.h"
// Interface core
void interfaceLoop(const InputState &input);
// Actions
void toggleAlarm();
void changeTimeZone();
void changeWeatherRegion();
// Callbacks implemented in main.cpp
void displayText(int x, int y, const std::string &text);
int getCounter(); // 1=OK, 2=Next, 3=Prev
void handleEncoderInput();

enum MenuOption {
  MENU_SELECTOR = -1,
  MENU_CLOCK,
  MENU_ALARM,
  MENU_SET_REGION,
  MENU_STOPWATCH,
  MENU_SET_TIMEZONE,
  MENU_COUNT
}; // === Menu Options ===

constexpr int MENU_COUNT_CONST = static_cast<int>(MENU_COUNT);

extern int currentMenu;
extern Adafruit_MPU6050 mpu;
#endif