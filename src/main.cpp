#include "interface.h"
#include <SPIFFS.h>
#include "esp_wpa2.h"
#include <RTClib.h>   // ✅ DS1307 RTC
#include <Wire.h>
#include "angry.h"
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
uint8_t ucFrameBuffer[(128*64) + ((128 * 64)/8)];

const int alarmChordCount = 4;
const float* alarmChords[] = {Cmaj7, G, Am7, Fmaj7};
const int alarmChordLens[] = {4, 3, 4, 4};

const unsigned long ntpSyncInterval = 6UL * 60UL * 60UL * 1000UL; // 6 hours

// === Global Display and RTC Objects ===
Adafruit_SSD1306 display(128, 64, &Wire);
Adafruit_MPU6050 mpu;
RTC_DS1307 extRTC;  // ✅ External DS1307 RTC

// === WiFi Credentials ===
const char* personalSSID     = "Wokwi-GUEST";
const char* personalPassword = "";

const char* instSSID     = "KIIT-WIFI-NET.";
const char* instUsername = "2206290"; 
const char* instPassword = "qT7bqcDx";

// === Function Prototypes ===
void displayText(int x, int y, const std::string &text) {
  display.setCursor(x, y);
  display.print(text.c_str());
}

extern const uint8_t _31[]; // GIF frame data for boot animation
extern const size_t _31_len;

// === RTC Init ===
void initRTC() {
  if (!extRTC.begin()) {
    Serial.println("❌ DS1307 not found");
    while(1) delay(100);
  } else {
    Serial.println("✅ DS1307 RTC OK");
    if (!extRTC.isrunning()) {
      Serial.println("🕒 DS1307 not running, setting to compile time...");
      extRTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
    } else {
      DateTime rtcTime = extRTC.now();
      Serial.printf("📅 DS1307 Current Time: %04d-%02d-%02d %02d:%02d:%02d\n",
        rtcTime.year(), rtcTime.month(), rtcTime.day(),
        rtcTime.hour(), rtcTime.minute(), rtcTime.second());
    }
  }
}

// === Sync ESP RTC & DS1307 with NTP ===
void syncRTCWithNTP() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 5000)) {
    Serial.printf("✅ NTP Sync Success: %04d-%02d-%02d %02d:%02d:%02d\n",
      timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    // Save to DS1307 for persistence
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
    Serial.println("💾 Time saved to DS1307 RTC");
  } else {
    Serial.println("❌ NTP sync failed - will use DS1307 fallback");
  }
}

// === Get Time from NTP or DS1307 fallback ===
bool getTimeWithFallback(struct tm &timeinfo) {
  // Try NTP first (only if WiFi is connected)
  if (WiFi.status() == WL_CONNECTED && getLocalTime(&timeinfo, 1000)) {
    return true;
  }

  // Fallback to DS1307 if NTP unavailable
  // Serial.println("⚠️ NTP unavailable, reading from DS1307...");
  DateTime now = extRTC.now();

  // Validate DS1307 time (sanity check: year should be >= 2024)
  if (now.year() < 2024) {
    //Serial.printf("⚠️ DS1307 has invalid time (%04d), waiting for NTP...\n", now.year());
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

  // Serial.printf("📅 Using DS1307 Time: %04d-%02d-%02d %02d:%02d:%02d\n",
  //   now.year(), now.month(), now.day(),
  //   now.hour(), now.minute(), now.second());

  return true;
}

// === WiFi Connection Helpers ===
bool connectPersonalWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  Serial.printf("📡 Connecting to Personal WiFi: %s\n", personalSSID);
  WiFi.begin(personalSSID, personalPassword);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 8000) {
    Serial.print(".");
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Personal WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("\n❌ Personal WiFi Failed");
  return false;
}

bool connectInstitutionWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  Serial.printf("🏫 Connecting to Institution WiFi: %s\n", instSSID);

  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)instUsername, strlen(instUsername));
  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)instUsername, strlen(instUsername));
  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)instPassword, strlen(instPassword));
  esp_wifi_sta_wpa2_ent_enable();
  WiFi.begin(instSSID);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    Serial.print(".");
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Institution WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("\n❌ Institution WiFi Failed");
  return false;
}

// === SPIFFS / Data directory helpers ===
void initSPIFFS() {
    if (!SPIFFS.begin(true)) {
        Serial.println("❌ SPIFFS mount failed!");
        while(1) delay(10);
    }
    Serial.println("✅ SPIFFS mounted successfully.");
}

void listFiles(const char* path = "/") {
    Serial.printf("📂 Listing files in: %s\n", path);
    File root = SPIFFS.open(path);
    if (!root || !root.isDirectory()) {
        Serial.println("❌ Failed to open directory");
        return;
    }
    File file = root.openNextFile();
    while(file) {
        Serial.print("  - ");
        Serial.print(file.name());
        Serial.print("  \t");
        Serial.println(file.size());
        file = root.openNextFile();
    }
}

// bool readFile(const char* path, uint8_t* buffer, size_t bufSize, size_t &readBytes) {
//     File f = SPIFFS.open(path, "r");
//     if (!f) {
//         Serial.printf("❌ Failed to open file: %s\n", path);
//         return false;
//     }
//     readBytes = f.read(buffer, bufSize);
//     Serial.printf("✅ Read %u bytes from %s\n", readBytes, path);
//     f.close();
//     return true;
// }

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

void playAnimation() {
  for (int i = 0; i < ANGRY_FRAME_COUNT; i++) {
    display.clearDisplay();
    display.drawBitmap(0, 0, angry_frames[i], ANGRY_WIDTH, ANGRY_HEIGHT, 1);
    display.display();
    delay(angry_delays[i]);  // use per-frame delay
  }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // === SPIFFS disabled for Wokwi simulation (no SPIFFS support) ===
    // Mount SPIFFS first
    // initSPIFFS();
    // Serial.println("\n📂 Animation files available:");
    // listFiles("/data");
    // 
    // // Initialize animation frame counts dynamically by scanning folders
    // initializeAnimationFrameCounts();

    // Display setup
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();

    Wire.begin(I2C_SDA, I2C_SCL);
    initRTC();

    // Prepare motion sensor for idle detection
    setupMotionSensor();

    // WiFi & NTP Sync
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

    // Boot animation
    playAnimation();
    Serial.println("✅ Boot animation done");

    fetchWeather();

    Serial.println("Buzzer ON");
    tone(BUZZER_PIN,1000);
    delay(1000);
    noTone(BUZZER_PIN);
    Serial.println("Buzzer OFF");

    if (!mpu.begin(0x69)) {
        Serial.println("❌ MPU6050 not found");
        while(1) delay(10);
    }

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

void loop() {
    if(inIdleAnimation){
        playScheduledAnimation();
        return;
    }

    if((millis()-lastMotionTime>idleTimeout) && !inIdleAnimation){
        Serial.println("💤 Timeout passed. Entering idle mode.");
        inIdleAnimation = true;
        return;
    }

    // Get smooth current time every loop (no NTP call, just system time)
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Occasionally verify time source is still valid (every 60 seconds)
    if(millis() - lastTimeVerifyTime >= TIME_VERIFY_INTERVAL) {
        getTimeWithFallback(cachedTimeinfo);  // Verify and sync if needed
        lastTimeVerifyTime = millis();
    }

    InputState currentInput = readUserInput();

    if(!inIdleAnimation){
        interfaceLoop(currentInput);

        const TimeZone &tz = timeZones[selectedTimeZoneIndex];
        time_t t = mktime(&timeinfo);
        t += (time_t)round(tz.offsetHours * 3600);  // Preserve fractional hours (e.g., IST +5.5)
        localtime_r(&t, &timeinfo);

        if(alarmEnabled){
            int secondsUntilAlarm = (alarmHour*60+alarmMinute)*60
                                  - (timeinfo.tm_hour*60+timeinfo.tm_min)*60
                                  - timeinfo.tm_sec;
            if(secondsUntilAlarm>0 && secondsUntilAlarm<=10){
                if(inIdleAnimation){
                    Serial.println("⏰ Exiting idle animation: alarm in 10 seconds!");
                    inIdleAnimation=false;
                    display.clearDisplay();
                    display.display();
                }
                idleBlockedForAlarm=true;
            } else idleBlockedForAlarm=false;
        }

        checkAndTriggerAlarm(timeinfo);
    }

    if(WiFi.status()==WL_CONNECTED && millis()-lastNTPSync>ntpSyncInterval){
        syncRTCWithNTP();
        lastNTPSync=millis();
    }

    if(alarmBeeping){
        // Turn off alarm with RST button
        if(currentInput.rstPressed){
            Serial.println("🔇 Alarm turned off by RST button");
            alarmBeeping=false;
            alarmChordIndex=0;
        } else if(millis()-alarmStartTime<60000){
            if(millis()-lastAlarmNoteTime>500){
                playChord(alarmChords[alarmChordIndex], alarmChordLens[alarmChordIndex]);
                alarmChordIndex=(alarmChordIndex+1)%alarmChordCount;
                lastAlarmNoteTime=millis();
            }
        } else {
            alarmBeeping=false;
            alarmChordIndex=0;
        }
    }
}
