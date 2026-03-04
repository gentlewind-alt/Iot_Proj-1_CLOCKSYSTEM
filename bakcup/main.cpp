#include "interface.h"
#include "31.h"
#include "esp_wpa2.h"
#include <RTClib.h>   // ✅ DS1307 RTC
#include <Wire.h>

// Alarm sound state
extern bool inIdleAnimation;
extern bool shaking;
extern int selectedTimeZoneIndex;
extern unsigned long alarmStartTime;
extern volatile bool alarmBeeping;

bool idleBlockedForAlarm = false;
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
// Personal WiFi (WPA2-PSK)
const char* personalSSID     = "Wokwi-GUEST";
const char* personalPassword = "";

// Institution WiFi (WPA2-Enterprise)
const char* instSSID     = "KIIT-WIFI-NET.";
const char* instUsername = "2206290"; 
const char* instPassword = "qT7bqcDx";

// == Function Prototypes ===
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
  } else {
    Serial.println("✅ DS1307 RTC OK");
    if (!extRTC.isrunning()) {
      Serial.println("🕒 DS1307 not running, setting to compile time...");
      extRTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }
}

// === Sync ESP RTC & DS1307 with NTP ===
void syncRTCWithNTP() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 5000)) {
    Serial.printf("✅ NTP Time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    // Save to DS1307
    extRTC.adjust(DateTime(
      timeinfo.tm_year + 1900,
      timeinfo.tm_mon + 1,
      timeinfo.tm_mday,
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec
    ));

    lastNTPSync = millis();
  } else {
    Serial.println("❌ NTP sync failed");
  }
}

// === Get Time from ESP32 RTC or DS1307 fallback ===
bool getTimeWithFallback(struct tm &timeinfo) {
  if (getLocalTime(&timeinfo)) {
    return true;
  }
  DateTime now = extRTC.now();
  timeinfo.tm_year = now.year() - 1900;
  timeinfo.tm_mon  = now.month() - 1;
  timeinfo.tm_mday = now.day();
  timeinfo.tm_hour = now.hour();
  timeinfo.tm_min  = now.minute();
  timeinfo.tm_sec  = now.second();
  mktime(&timeinfo);
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

// GIF loader
void * GIFOpenFile(const char *filename, int32_t *pSize) {
  *pSize = _31_len;
  return (void *)_31;
}

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  memcpy(pBuf, (const uint8_t *)pFile->fHandle + pFile->iPos, iLen);
  pFile->iPos += iLen;
  return iLen;
}

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
  pFile->iPos = iPosition;
  return iPosition;
}

void playEmojiBootAnimation() {
  gif.begin(LITTLE_ENDIAN_PIXELS);
  gif.open((uint8_t *)_31, sizeof(_31), GIFDraw);
  Serial.println("🎬 Boot animation playing...");
  while (gif.playFrame(true, NULL)) {
    display.display();
    delay(1);
  }
  gif.close();
  Serial.println("✅ Boot animation complete.");
}

void setup() {
  Serial.begin(115200);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();

  Wire.begin(I2C_SDA, I2C_SCL);
  initRTC(); // ✅ Init DS1307 first

  // ===== Try WiFi Connections =====
  bool wifiConnected = false;
  if (connectPersonalWiFi()) {
    wifiConnected = true;
  } else if (connectInstitutionWiFi()) {
    wifiConnected = true;
  }

  if (wifiConnected) {
    syncRTCWithNTP(); // ✅ Sync both RTCs
  } else {
    Serial.println("📴 No WiFi available, using DS1307 fallback");
  }

  playEmojiBootAnimation();
  fetchWeather();

  Serial.println("Buzzer ON");
  tone(BUZZER_PIN, 1000);
  delay(1000);
  noTone(BUZZER_PIN);
  Serial.println("Buzzer OFF");

  if (!mpu.begin(0x69)) {
    Serial.println("❌ MPU6050 not found");
    while (1) delay(10);
  }

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
void loop() {
  if (inIdleAnimation) {
    playScheduledAnimation();
    return;
  }

  if ((millis() - lastMotionTime > idleTimeout) && !inIdleAnimation) {
    Serial.println("💤 Timeout passed. Entering idle mode.");
    inIdleAnimation = true;
    return;
  }

  if (!isIdlePlaying) {
    showClockPage();
  }

  struct tm timeinfo;
  getTimeWithFallback(timeinfo); // ✅ Now uses fallback

  InputState currentInput = readUserInput();

  if (!inIdleAnimation) {
    interfaceLoop(currentInput);

    const TimeZone& tz = timeZones[selectedTimeZoneIndex];
    timeinfo.tm_hour += (int)tz.offsetHours;
    mktime(&timeinfo);

    if (alarmEnabled) {
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
    showClockPage();
  }

  if (WiFi.status() == WL_CONNECTED && millis() - lastNTPSync > ntpSyncInterval) {
    syncRTCWithNTP();
    lastNTPSync = millis();
  }

  if (alarmBeeping) {
    if (millis() - alarmStartTime < 60000) {
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
