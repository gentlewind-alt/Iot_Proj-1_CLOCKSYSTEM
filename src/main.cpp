#include "interface.h"
#include <LittleFS.h>
#include <RTClib.h>   // ✅ DS1307 RTC
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include "animation.h"  // ✅ Animation system (.bin files)
#include "filesystem.h" // ✅ Filesystem utilities

// === NEW SENSOR MODULES (Phase 2) ===
#include "hcsr04.h"           // Distance sensor
#include "mpu6050_angle.h"    // Angle/level sensor
#include "pir_motion.h"       // Motion detector
#include "distance_widget.h"  // Distance display widget
#include "angle_widget.h"     // Angle display widget
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
// #define FRAME_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 8)

// Alarm sound state
extern bool inIdleAnimation;
extern unsigned long alarmStartTime;
extern volatile bool alarmBeeping;

bool idleBlockedForAlarm = false;

const int alarmChordCount = 4;
const float* alarmChords[] = {Cmaj7, G, Am7, Fmaj7};
const int alarmChordLens[] = {4, 3, 4, 4};

// === Global Display and RTC Objects ===
Adafruit_SSD1306 display(128, 64, &Wire);
Adafruit_MPU6050 mpu;
RTC_DS1307 extRTC;  // ✅ External DS1307 RTC
Adafruit_NeoPixel statusLED(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

void setStatusColor(uint8_t r, uint8_t g, uint8_t b) {
    statusLED.setPixelColor(0, statusLED.Color(r, g, b));
    statusLED.show();
}

// === NEW SENSOR OBJECTS (Phase 2) ===
DistanceSensor* distanceSensor = nullptr;      // HC-SR04 ultrasonic
LevelSensor* levelSensor = nullptr;            // MPU6050 angle/level
MotionDetector* motionDetector = nullptr;      // PIR motion detector

// === NEW SENSOR WIDGETS (Phase 2) ===
DistanceWidget* distanceWidget = nullptr;      // Distance display
AngleWidget* angleWidget = nullptr;            // Angle/level display

// === Function Prototypes ===
void displayText(int x, int y, const std::string &text) {
  display.setCursor(x, y);
  display.print(text.c_str());
}

// Animation system initialized in setup()

// === RTC Init ===
void initRTC() {
  if (!extRTC.begin()) {
    Serial.println("❌ DS1307 not found");
    while(1) delay(100);
  } else {
    Serial.println("✅ DS1307 RTC OK");
    
    DateTime now = extRTC.now();
    DateTime compiled = DateTime(F(__DATE__), F(__TIME__));
    
    // If RTC is stopped OR the current time is behind the build time, update it
    if (!extRTC.isrunning() || now.unixtime() < compiled.unixtime()) {
      Serial.println("🕒 RTC time is invalid or older than compile time. Syncing...");
      extRTC.adjust(compiled);
      now = extRTC.now(); // Refresh after adjustment
    }
    
    Serial.printf("📅 DS1307 Current Time: %04d-%02d-%02d %02d:%02d:%02d\n",
      now.year(), now.month(), now.day(),
      now.hour(), now.minute(), now.second());
  }
}

// === Get Time from RTC ===
bool getTimeWithFallback(struct tm &timeinfo) {
  // Use DS1307 RTC
  DateTime now = extRTC.now();

  // Validate DS1307 time (sanity check: year should be >= 2024)
  if (now.year() < 2024) {
    return false;
  }

  // Convert DS1307 time to struct tm
  timeinfo.tm_year = now.year() - 1900;
  timeinfo.tm_mon  = now.month() - 1;
  timeinfo.tm_mday = now.day();
  timeinfo.tm_hour = now.hour();
  timeinfo.tm_min  = now.minute();
  timeinfo.tm_sec  = now.second();
  mktime(&timeinfo);

  return true;
}

// Play boot animation from SPIFFS/LittleFS
void playBootAnimation() {
  // Try to play 0.bin if it exists
  if (globalAnimPlayer && playAnimationBlocking("/dasai.bin", 40, false)) {
    Serial.println("✅ Boot animation played from /dasai.bin");
  } else {
    // Fallback: show startup message
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 20);
    display.println("Clock Booting...");
    display.display();
    delay(1000);
  }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Initialize NeoPixel
    statusLED.begin();
    statusLED.setBrightness(50);
    setStatusColor(50, 50, 50); // White-ish on boot

    // 1. Initialize I2C first with correct pins (SDA=5, SCL=6)
    Wire.begin(I2C_SDA, I2C_SCL);

    // Initialize filesystem and animation system
    initFilesystem();
    initAnimationSystem(&display);
    
    // Debug: List files to Serial
    listFilesInDirectory("/", 0);

    // 2. Initialize display (will use existing Wire on pins 5, 6)
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();

    initRTC();

    // Prepare motion sensor for idle detection
    setupMotionSensor();

    // Verify we have valid time from somewhere
    struct tm testTime;
    if (!getTimeWithFallback(testTime)) {
        Serial.println("❌ CRITICAL: No valid time source available!");
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println("Time Error:");
        display.println("No valid RTC");
        display.display();
        delay(2000);
    }

    // Boot animation (.bin file system)
    playBootAnimation();
    Serial.println("✅ Boot sequence complete");

    Serial.println("Buzzer ON");
    tone(BUZZER_PIN,1000);
    delay(1000);
    noTone(BUZZER_PIN);
    Serial.println("Buzzer OFF");

    if (!mpu.begin(0x69)) {
        Serial.println("❌ MPU6050 not found");
        while(1) delay(10);
    }


    // === Initialize Sensors (Phase 2) ===
    // Distance Sensor (HC-SR04)
    distanceSensor = new DistanceSensor(TRIG_PIN, ECHO_PIN);
    distanceSensor->begin();
    Serial.println("✅ Distance sensor initialized");
    
    // Angle/Level Sensor (MPU6050)
    levelSensor = new LevelSensor(&mpu);
    levelSensor->begin();
    Serial.println("⚖️ Calibrating angle sensor (keep clock flat)...");
    levelSensor->calibrate();
    Serial.println("✅ Angle sensor initialized and calibrated");
    
    // Motion Detector (PIR)
    motionDetector = new MotionDetector(PIR_PIN);
    motionDetector->begin();
    Serial.println("✅ Motion detector initialized");
    
    // === Initialize Widgets (Phase 2) ===
    distanceWidget = new DistanceWidget(distanceSensor, &display);
    angleWidget = new AngleWidget(levelSensor, &display);
    Serial.printf("🔍 Sensor Pointers: DS=%p, LS=%p, DW=%p, AW=%p\n", 
                  distanceSensor, levelSensor, distanceWidget, angleWidget);
    Serial.println("✅ Sensor widgets initialized");

    // Pins
    pinMode(pinCLK, INPUT_PULLUP);
    pinMode(pinDT, INPUT_PULLUP);
    pinMode(pinSW, INPUT_PULLUP);
    pinMode(PIR_PIN, INPUT);
    pinMode(pinRST, INPUT_PULLUP);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    attachInterrupt(digitalPinToInterrupt(pinCLK), handleEncoderInput, FALLING);

    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.display();

    Serial.println("✅ Setup complete");
}

unsigned long lastSensorLog = 0;
unsigned long lastTimeVerifyTime = 0;
const unsigned long TIME_VERIFY_INTERVAL = 60000;  // Verify time source every 60 seconds, not every loop
struct tm cachedTimeinfo = {};

void updateStatusLED() {
    static uint32_t lastLEDUpdate = 0;
    static uint8_t hue = 0;
    
    // Priority 1: Alarm Ringing (Red Blinking)
    if (alarmBeeping) {
        if ((millis() / 250) % 2 == 0) {
            setStatusColor(255, 0, 0); // Red
        } else {
            setStatusColor(0, 0, 0);   // Off
        }
        return;
    }

    // Priority 2: Alarm Editing (Orange)
    if (alarmEditing) {
        setStatusColor(255, 100, 0); // Orange
        return;
    }

    // Priority 3: Idle Animation (Rainbow Gradient)
    if (inIdleAnimation) {
        if (millis() - lastLEDUpdate > 20) {
            hue++;
            // Simple rainbow cycle
            uint32_t color = statusLED.ColorHSV(hue * 256, 255, 150);
            statusLED.setPixelColor(0, color);
            statusLED.show();
            lastLEDUpdate = millis();
        }
        return;
    }

    // Priority 4: Specific Menus
    switch (currentMenu) {
        case MENU_DISTANCE:
            if (distanceSensor) {
                uint16_t d = distanceSensor->getSmoothedDistance();
                if (d > 0 && d < 50) {
                    // Green to Red based on distance
                    setStatusColor(255 - (d * 5), d * 5, 0); 
                } else {
                    setStatusColor(0, 50, 0); // Dim Green
                }
            }
            break;
            
        case MENU_ALARM:
            if (alarmEnabled) {
                setStatusColor(0, 0, 100); // Blue (ON)
            } else {
                setStatusColor(20, 20, 20); // Dim White (OFF)
            }
            break;

        case MENU_STOPWATCH:
            if (running) {
                setStatusColor(0, 255, 255); // Cyan
            } else {
                setStatusColor(0, 50, 50);   // Dim Cyan
            }
            break;

        case MENU_ANGLE:
            setStatusColor(100, 0, 100); // Purple
            break;

        default:
            // Normal Clock Mode
            if (alarmEnabled) {
                setStatusColor(0, 20, 50); // Very dim blue
            } else {
                setStatusColor(5, 5, 5); // Faint white
            }
            break;
    }
}

void loop() {
    // 0. Update Status LED
    updateStatusLED();

    // 1. Always check for user activity to handle idle transitions
    checkForUserActivity();

    // 2. Always update current time info (use DS1307 RTC, not ESP32 internal time)
    struct tm timeinfo;
    if (!getTimeWithFallback(timeinfo)) {
        // If time is invalid, skip alarm check this loop
        return;
    }

    // Occasionally verify RTC time (every 60s)
    if(millis() - lastTimeVerifyTime >= TIME_VERIFY_INTERVAL) {
        getTimeWithFallback(cachedTimeinfo);
        lastTimeVerifyTime = millis();
    }

    // 3. Always check if alarm should trigger (Crucial fix!)
    if (alarmEnabled) {
        // Pre-check: exit idle mode 10s before alarm starts
        int secondsUntilAlarm = (alarmHour * 60 + alarmMinute) * 60
                              - (timeinfo.tm_hour * 60 + timeinfo.tm_min) * 60
                              - timeinfo.tm_sec;
                              
        if (secondsUntilAlarm > 0 && secondsUntilAlarm <= 10) {
            if (inIdleAnimation) {
                Serial.println("⏰ Exiting idle animation: alarm in 10 seconds!");
                inIdleAnimation = false;
                display.clearDisplay();
                display.display();
            }
            idleBlockedForAlarm = true;
        } else {
            idleBlockedForAlarm = false;
        }
    }
    
    checkAndTriggerAlarm(timeinfo);

    // 4. Handle Idle Animation (Non-blocking)
    if (inIdleAnimation) {
        playScheduledAnimation();
        // Even in idle, we must handle alarm beeping if it triggers
        if (!alarmBeeping) return; 
    }

    // 5. Handle Inactivity Timeout
    if ((millis() - lastMotionTime > idleTimeout) && !inIdleAnimation && !alarmBeeping && !idleBlockedForAlarm) {
        Serial.println("💤 Timeout passed. Entering idle mode.");
        inIdleAnimation = true;
        return;
    }

    // 6. Update Sensor States
    if (motionDetector) {
        motionDetector->update();
    }

    // 7. Handle User Interface and Alarm Dismissal
    InputState currentInput = readUserInput();

    if (!inIdleAnimation) {
        interfaceLoop(currentInput);
    }

    if (alarmBeeping) {
        // Turn off alarm with RST button
        if (currentInput.rstPressed) {
            Serial.println("🔇 Alarm turned off by RST button");
            alarmBeeping = false;
            alarmChordIndex = 0;
            alarmEnabled = !alarmEnabled;
            alarmOffAnimation();
        }
        // Stop alarm if motion detected
        else if (motionDetector && motionDetector->isMotionDetected()) {
            Serial.println("🔇 Alarm turned off by motion detection!");
            alarmBeeping = false;
            alarmChordIndex = 0;
            display.clearDisplay();
            display.setTextSize(1);
            display.setCursor(20, 25);
            display.println("Motion Detected");
            display.setCursor(15, 35);
            display.println("Alarm Stopped");
            display.display();
            delay(2000);
        }
        else if (millis() - alarmStartTime < 60000) {
            if (millis() - lastAlarmNoteTime > 500) {
                playChord(alarmChords[alarmChordIndex], alarmChordLens[alarmChordIndex]);
                alarmChordIndex = (alarmChordIndex + 1) % alarmChordCount;
                lastAlarmNoteTime = millis();
            }
        } else {
            alarmBeeping = false;
            alarmChordIndex = 0;
        }
    }
}
