#include "interface.h"
#include <LittleFS.h>
#include "esp_wpa2.h"
#include <WiFi.h>
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
extern bool shaking;
extern int selectedTimeZoneIndex;
extern unsigned long alarmStartTime;
extern volatile bool alarmBeeping;

bool idleBlockedForAlarm = false;
bool ntpSyncedOnce = false;
bool wifiInitialized = false;  // Track if WiFi has been initialized

const int alarmChordCount = 4;
const float* alarmChords[] = {Cmaj7, G, Am7, Fmaj7};
const int alarmChordLens[] = {4, 3, 4, 4};

const unsigned long ntpSyncInterval = 6UL * 60UL * 60UL * 1000UL; // 6 hours

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

// === WiFi Credentials ===
const char* personalSSID     = "Airtel_Nanni";
const char* personalPassword = "Samarth#2006";

const char* instSSID     = "KIIT-WIFI-NET.";
const char* instUsername = "2206290"; 
const char* instPassword = "qT7bqcDx";

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

// === Sync ESP RTC & DS1307 with NTP ===
bool syncRTCWithNTP() {
  // IST = UTC+5:30 (19800 seconds)
  const long utcOffsetSec = 5 * 3600 + 30 * 60;
  const int daylightOffsetSec = 0;
  configTime(utcOffsetSec, daylightOffsetSec, "pool.ntp.org");

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 5000)) {
    Serial.printf("✅ NTP Sync Success (IST): %04d-%02d-%02d %02d:%02d:%02d\n",
                  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    // Save to DS1307 for persistence (store as IST)
    extRTC.adjust(DateTime(
      timeinfo.tm_year + 1900,
      timeinfo.tm_mon + 1,
      timeinfo.tm_mday,
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec
    ));

    ntpSyncedOnce = true;
    lastNTPSync = millis();
    Serial.println("💾 Time saved to DS1307 RTC (IST)");
    // Power down Wi‑Fi after successful sync
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return true;
  } else {
    Serial.println("❌ NTP sync failed – will use DS1307 fallback");
    // Ensure Wi‑Fi is turned off even on failure
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return false;
  }
}

// === Get Time from RTC (WiFi/NTP disabled) ===
bool getTimeWithFallback(struct tm &timeinfo) {
  // WiFi disabled to save RAM - use DS1307 RTC only
  // if (WiFi.status() == WL_CONNECTED && getLocalTime(&timeinfo, 1000)) {
  //   return true;
  // }

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

// === WiFi Connection Helpers (DISABLED - WiFi removed) ===
// These are stubbed since WiFi.h is no longer included
// Uncomment if you re-enable WiFi by:
// 1. Adding #include <WiFi.h> back to interface.h
// 2. Using a board with more RAM (ESP32, not ESP32-C3)

bool connectPersonalWiFi() {
  Serial.printf("🌐 Connecting to Personal WiFi: %s\n", personalSSID);
  WiFi.begin(personalSSID, personalPassword);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connected to Personal WiFi");
    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\n❌ Failed to connect to Personal WiFi");
    return false;
  }
}

bool connectInstitutionWiFi() {
  Serial.printf("🌐 Connecting to Institution WiFi: %s\n", instSSID);
  
  // Configure WPA2 Enterprise
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)instUsername, strlen(instUsername));
  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)instUsername, strlen(instUsername));
  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)instPassword, strlen(instPassword));
  esp_wifi_sta_wpa2_ent_enable();
  
  WiFi.begin(instSSID);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connected to Institution WiFi");
    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\n❌ Failed to connect to Institution WiFi");
    return false;
  }
}

// === RTC Sync (no NTP available without WiFi) ===
// void syncRTCWithNTP() {
//   // NTP sync disabled - WiFi not available
// }

// SPIFFS initialization moved to filesystem.cpp (initFilesystem function)

// Old SPIFFS helper removed - use filesystem.cpp functions instead

// File I/O helpers removed - use filesystem.cpp functions instead

// const char* bootAnimPath = "/dasai2.pbin";

// // Read BINPACK header
// static bool readPackedHeader(File &f, uint16_t &w, uint16_t &h, uint32_t &count) {
//     char magic[8];
//     if (f.readBytes(magic, 8) != 8) return false;
//     if (memcmp(magic, "BINPACK\0", 8) != 0) return false;
//     if (f.readBytes((char*)&w, 2) != 2) return false;
//     if (f.readBytes((char*)&h, 2) != 2) return false;
//     if (f.readBytes((char*)&count, 4) != 4) return false;
//     return true;
// }

// Play boot animation from SPIFFS
// void playBootAnimation() {
//     if (!SPIFFS.exists(bootAnimPath)) {
//         Serial.printf("❌ Boot animation missing: %s\n", bootAnimPath);
//         return;
//     }

//     File f = SPIFFS.open(bootAnimPath, "r");
//     if (!f) {
//         Serial.printf("❌ Failed to open: %s\n", bootAnimPath);
//         return;
//     }

//     uint16_t w=0, h=0; uint32_t count=0;
//     if (!readPackedHeader(f, w, h, count)) {
//         Serial.printf("❌ Invalid packed animation: %s\n", bootAnimPath);
//         f.close();
//         return;
//     }

//     const size_t bytesPerFrame = (w * h)/8;
//     static uint8_t frame[FRAME_SIZE];
//     for(uint32_t i=0; i<count; i++){
//         size_t n = f.read(frame, bytesPerFrame);
//         if(n != bytesPerFrame) break;
//         display.clearDisplay();
//         display.drawBitmap(0,0,frame,SCREEN_WIDTH,SCREEN_HEIGHT,1);
//         display.display();
//         delay(80);
//     }
//     f.close();
// }

// Play boot animation from SPIFFS/LittleFS
void playBootAnimation() {
  // Try to play 0.bin if it exists
  if (globalAnimPlayer && playAnimationBlocking("/0.bin", 40, false)) {
    Serial.println("✅ Boot animation played from /0.bin");
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

    // 🚨 WiFi DISABLED - BSS too large for ESP32-C3 (320 KB RAM vs 935 KB WiFi stack)
    // Using only DS1307 RTC for time keeping
    // If you need WiFi: upgrade to ESP32 with more RAM or use external PSRAM
    
    // WiFi & NTP Sync (Lightweight Option B)
    bool wifiConnected = false;
    if (connectPersonalWiFi()) {
        wifiConnected = true;
    } else if (connectInstitutionWiFi()) {
        wifiConnected = true;
    }
    
    if (wifiConnected) {
        syncRTCWithNTP();
        if (!ntpSyncedOnce) {
            Serial.println("⚠️ NTP sync failed on startup, DS1307 will be used for time");
        }
    } else {
        Serial.println("📴 No WiFi available - using DS1307 RTC for time");
    }

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

    // Boot animation (.h file system)
    playBootAnimation();
    Serial.println("✅ Boot sequence complete");

    // fetchWeather();  // ⚠️ DISABLED: Requires WiFi/HTTPClient

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
