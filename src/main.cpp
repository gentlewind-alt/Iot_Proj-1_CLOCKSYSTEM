#include "interface.h"

// Alarm sound state
int alarmChordIndex = 0;
unsigned long lastAlarmNoteTime = 0;
const int alarmChordCount = 4;
const float* alarmChords[] = {Cmaj7, G, Am7, Fmaj7};
const int alarmChordLens[] = {4, 3, 4, 4};
unsigned long lastNTPSync = 0;
const unsigned long ntpSyncInterval = 6UL * 60UL * 60UL * 1000UL; // 6 hours
// === Global Display and RTC Objects ===
Adafruit_SSD1306 display(128, 64, &Wire);
RTC_DS1307 rtc;
Adafruit_MPU6050 mpu;
// == Function Prototypes ===
void displayText(int x, int y, const std::string &text) {
  display.setCursor(x, y);
  display.print(text.c_str());
}

const char* ssid = "Wokwi-GUEST"; // WiFi SSID
const char* password = "";

void playEmojiBootAnimation() {
  for (int i = 0; i < 49; i++) {   // assuming 90 frames total
    display.clearDisplay();;
    display.drawBitmap(0, 0, temp_intro_allArray[i], 128, 64, SSD1306_WHITE);
    display.display();
    delay(100); // Adjust delay for speed of animation
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("RTC not found!");
    while (1);
  }

  // Initialize WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ");
  Serial.println(ssid);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi connection failed!");
    Serial.println("Status Code: " + String(WiFi.status()));
  }

  fetchWeather();

  if (WiFi.status() == WL_CONNECTED) {
    syncRTCWithNTP();
  }

  Serial.println("Buzzer ON");
  tone(BUZZER_PIN, 1000);
  delay(1000);
  Serial.println("Buzzer OFF");
  noTone(BUZZER_PIN);

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!mpu.begin(0x69)) {
    Serial.println("❌ MPU6050 not found at 0x69");
    while (1) delay(10);
  }
  Serial.println("✅ MPU6050 initialized");
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.display();

  // === Emoji Boot Expression ===
  //playEmojiBootAnimation();
  Serial.println("✅ Emoji boot animation complete!");
  // setupMotionSensor();
  
  pinMode(pinCLK, INPUT_PULLUP);
  pinMode(pinDT, INPUT_PULLUP);
  pinMode(pinSW, INPUT_PULLUP);
  pinMode(PIR_PIN, INPUT);
  pinMode(pinRST, INPUT_PULLUP);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(pinCLK), handleEncoderInput, FALLING);

  // pixel.begin();
  // pixel.setBrightness(100);
  // lightPixel(pixel.Color(30, 129, 176));
  attachInterrupt(digitalPinToInterrupt(pinCLK), handleEncoderInput, FALLING);
}

void loop() {
    motionSensorLoop();

    // --- Exit idle animation 10 seconds before alarm ---
// At the top of loop()
  if (alarmEnabled && rtc.isrunning()) {
      DateTime now = rtc.now();
      const TimeZone& tz = timeZones[selectedTimeZoneIndex];
      DateTime localNow = now + TimeSpan((int)(tz.offsetHours * 3600));

      int secondsUntilAlarm = (alarmHour * 60 + alarmMinute) * 60
                            - (localNow.hour() * 60 + localNow.minute()) * 60
                            - localNow.second();

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

  if (inIdleAnimation) {
      orienConfig();
      display.display();
      return;
  }
  // Only do clock & interface work if not idle
  if (rtc.isrunning()) {
    DateTime now = rtc.now();
    const TimeZone& tz = timeZones[selectedTimeZoneIndex];
    DateTime localNow = now + TimeSpan((int)(tz.offsetHours * 3600));
    checkAndTriggerAlarm(localNow);
    showClockPage(now);   // ⏰ check and ring alarm
  } else {
    Serial.println("⚠ RTC not running or not responding");
  }
  interfaceLoop(); // Draw the default clock screen at boot

  if (WiFi.status() == WL_CONNECTED && millis() - lastNTPSync > ntpSyncInterval) {
    syncRTCWithNTP();
    lastNTPSync = millis();
  }

  if (alarmBeeping) {
      if (millis() - alarmStartTime < 60000) {
          if (millis() - lastAlarmNoteTime > 500) { // Play next chord every 500ms
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

