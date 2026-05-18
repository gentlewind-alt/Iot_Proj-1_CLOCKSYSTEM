#include "interface.h"

extern Adafruit_SSD1306 display;
// Add this extern so main.cpp and interface.cpp share the same variable
bool menuselecting = false;
int currentMenu = MENU_CLOCK; // Default to clock
static int selectedMenuIndex = MENU_CLOCK;
static int lastMenu = -1; // Track the last menu for submenu logic
static int pendingMenuIndex = -1;
static unsigned long pendingMenuTime = 0;
static bool menuSwitchPending = false;
// === Global Variables ===
static unsigned long lastActionTime = 0;
bool running = false;
static bool editingAlarm = false;
static bool editingMinute = false;
bool alarmEditing = false;
static unsigned long lastRtcRead = 0;
static DateTime now;
bool menuChoice = false;
bool rotaryUsed = false; 
volatile int counter = 0; // Rotary encoder counter
unsigned long menuEntryTime = 0; // Add near the top, outside any function
static unsigned long lastRotaryActionTime = 0;
const unsigned long rotaryDebounceDelay = 150; // ms, adjust as needed

//int currentEmoji = 0;  // <<< Moved here for proper scoping if not global in emoji.cpp

// === Time Formatters ===
static std::string formatTime(int h, int m) {
  char b[6]; snprintf(b, sizeof(b), "%02d:%02d", h, m);
  return std::string(b);
}
static std::string formatTZ(int off) {
  char b[8]; snprintf(b, sizeof(b), "UTC%+d", off);
  return std::string(b);
}

// === Helpers ===
void toggleAlarm() { alarmEnabled = !alarmEnabled; }
const char* getMenuName(int index) {
  switch (index) {
    case MENU_CLOCK: return "Clock";
    case MENU_ALARM: return "Alarm";
    case MENU_STOPWATCH: return "Stopwatch";
    case MENU_DISTANCE: return "Distance";      // NEW: Distance sensor
    case MENU_ANGLE: return "Level";            // NEW: Angle/level sensor
    default: return "Unknown";
  }
}

// === Input Handling ===
volatile unsigned long lastEncoderTime = 0;
const unsigned long encoderDebounce = 2; // ms
const unsigned long inputSensitivityDelay = 100; // ms, debounce for alarm menu input handling

void IRAM_ATTR handleEncoderInput() {
    unsigned long now = millis();

    if (now - lastEncoderTime > encoderDebounce) {
        int dtValue = digitalRead(pinDT);
        if (dtValue == HIGH) {
            counter++;
        } else {
            counter--;
        }
        lastEncoderTime = now;
        rotaryUsed = true; // Set flag to indicate rotary was used
    } else {
        rotaryUsed = false; // Reset flag if debounce not passed
    }
}

int getCounter() {
    int result;
    noInterrupts();
    result = counter;
    interrupts();
    return result;
}

InputState readUserInput() {
    static int lastCounter = 0;
    static unsigned long lastSwTime = 0;
    static unsigned long lastRstTime = 0;
    static int lastSwState = HIGH;
    static int lastRstState = HIGH;
    static unsigned long swPressStartTime = 0;
    static unsigned long rstPressStartTime = 0;
    
    // Debounce constants
    const unsigned long BUTTON_DEBOUNCE_MS = 50;    // Time to wait after detecting LOW
    const unsigned long BUTTON_REPRESS_MIN_MS = 200; // Minimum time between presses
    
    int rotaryDirection = 0; 
    int currentCounter = getCounter();
    if (currentCounter != lastCounter) {
        if (currentCounter > lastCounter) {
            rotaryDirection = 1; // Clockwise action
        } else {
            rotaryDirection = -1; // Counter-clockwise action
        }
        lastCounter = currentCounter;
    }

    unsigned long currentTime = millis();
    int swPressed = 0;
    int rstPressed = 0;
    
    // Debounce SW button (improved)
    int swState = digitalRead(pinSW);
    if (swState == LOW && lastSwState == HIGH) {
        // Button pressed - record the start time
        swPressStartTime = currentTime;
    }
    // Check if button has been held LOW for debounce time AND enough time since last press
    if (swState == LOW && (currentTime - swPressStartTime) >= BUTTON_DEBOUNCE_MS && 
        (currentTime - lastSwTime) >= BUTTON_REPRESS_MIN_MS) {
        swPressed = 1;
        lastSwTime = currentTime;
        swPressStartTime = currentTime; // Reset to prevent repeated triggers
    }
    // Reset debounce if button released
    if (swState == HIGH && lastSwState == LOW) {
        swPressStartTime = 0;
    }
    lastSwState = swState;
    
    // Debounce RST button (improved consistency)
    int rstState = digitalRead(pinRST);
    if (rstState == LOW && lastRstState == HIGH) {
        // Button pressed - record the start time
        rstPressStartTime = currentTime;
    }
    // Check if button has been held LOW for debounce time AND enough time since last press
    if (rstState == LOW && (currentTime - rstPressStartTime) >= BUTTON_DEBOUNCE_MS && 
        (currentTime - lastRstTime) >= BUTTON_REPRESS_MIN_MS) {
        rstPressed = 1;
        lastRstTime = currentTime;
    }
    // Reset debounce if button released
    if (rstState == HIGH && lastRstState == LOW) {
        rstPressStartTime = 0;
    }
    lastRstState = rstState;

    InputState state = {rotaryDirection, swPressed, rstPressed};
    return state;
}

// === Main Menu Loop ===
void interfaceLoop(const InputState& input) {
    static int lastRotaryDirection = 0;
    static int lastDirection = 0;
    static unsigned long lastUIUpdateTime = 0;
    const unsigned long UI_UPDATE_INTERVAL = 50;  // Update display every 50ms, not input processing

    // --- Rotary on any menu: preview and switch after 2s (ALWAYS process, never throttle) ---
    if ((!editingAlarm && !editingMinute) && ((input.rotaryDirection == 1 || input.rotaryDirection == -1) && 
        lastRotaryDirection == 0) && (millis() - lastRotaryActionTime > rotaryDebounceDelay)){
        // Calculate new menu index
        int newIndex = currentMenu;
        if (input.rotaryDirection == 1) {
            newIndex = (currentMenu + 1) % MENU_COUNT;
        } else if (input.rotaryDirection == -1) {
            newIndex = (currentMenu - 1 + MENU_COUNT) % MENU_COUNT;
        }
        pendingMenuIndex = newIndex;
        pendingMenuTime = millis();
        menuSwitchPending = true;

        // Show menu name in center
        display.clearDisplay();
        std::string menuName = getMenuName(pendingMenuIndex);
        int textX = (128 - menuName.length() * 12) / 2;
        int textY = (64 - 16) / 2;
        display.setTextSize(2);
        display.setCursor(textX, textY);
        display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        display.print(menuName.c_str());
        display.display();
        lastRotaryDirection = input.rotaryDirection;
        return;
    }
    lastRotaryDirection = input.rotaryDirection;

    // If a menu switch is pending, wait 2 seconds before switching
    if (menuSwitchPending) {
        if (millis() - pendingMenuTime >= 500) {
            currentMenu = pendingMenuIndex;
            menuSwitchPending = false;
        } else {
            // Still showing preview, do not process menu logic
            return;
        }
    }

    // Throttle DISPLAY rendering only (not input processing)
    static bool shouldRender = true;
    if (millis() - lastUIUpdateTime >= UI_UPDATE_INTERVAL) {
        lastUIUpdateTime = millis();
        shouldRender = true;
    } else {
        shouldRender = false;
    }

    display.clearDisplay();

    switch (currentMenu) {
        case MENU_ALARM: {
            bool inIdleAnimation = false;
            // Draw a rounded box for the alarm time (centered, 128x64 screen)
            int boxX = 10, boxY = 8, boxW = 108, boxH = 48;
            display.drawRoundRect(boxX, boxY, boxW, boxH, 8, SSD1306_WHITE);

            // Label: "Alarm" or "Set Hour"/"Set Minute"
            std::string label = editingAlarm
                ? (editingMinute ? "Set Minute" : "Set Hour")
                : "Alarm";
            display.setTextSize(1);
            int labelX = boxX + (boxW - label.length() * 6) / 2;
            int labelY = boxY + 4;
            display.setCursor(labelX, labelY);
            display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
            display.print(label.c_str());

            // Alarm time, large and centered
            std::string timeStr = formatTime(alarmHour, alarmMinute);
            display.setTextSize(2);
            int timeW = 12 * 5; // 5 chars, 12px each at size 2
            int timeX = boxX + (boxW - timeW) / 2;
            int timeY = boxY + 16;
            display.setCursor(timeX, timeY);
            display.print(timeStr.c_str());
            display.setTextSize(1);

            // Alarm status (ON/OFF) as a small badge in the top-right of the box
            int badgeW = 28, badgeH = 12;
            int badgeX = boxX + boxW - badgeW - 4;
            int badgeY = boxY + 4;
            display.fillRoundRect(badgeX, badgeY, badgeW, badgeH, 4, alarmEnabled ? SSD1306_WHITE : SSD1306_BLACK);
            display.setTextColor(alarmEnabled ? SSD1306_BLACK : SSD1306_WHITE, alarmEnabled ? SSD1306_WHITE : SSD1306_BLACK);
            display.setCursor(badgeX + 4, badgeY + 2);
            display.print(alarmEnabled ? "ON" : "OFF");
            display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);

            // Process input (NOT throttled - always responsive)
            if (millis() - lastActionTime > 100) {
                if (!editingAlarm) {
                    // Not editing: toggle alarm ON/OFF with RST, enter hour set mode with SW
                    if (input.rstPressed) {
                        alarmEnabled = !alarmEnabled;
                        lastActionTime = millis();
                    }
                    else if (input.swPressed) {
                        editingAlarm = true;
                        editingMinute = false;
                        alarmEditing = true;
                        alarmEnabled = false; // Safeguard: turn off alarm while editing
                        lastActionTime = millis();
                    }
                } else if (!editingMinute) {
                    // Set hour: rotary changes hour, SW moves to minute set
                    if (input.rotaryDirection == 1 && lastDirection == 0) {
                        alarmHour = (alarmHour + 1) % 24;
                        lastActionTime = millis();
                    }
                    else if (input.rotaryDirection == -1 && lastDirection == 0) {
                        alarmHour = (alarmHour + 23) % 24;
                        lastActionTime = millis();
                    }
                    else if (input.swPressed) {
                        editingMinute = true;
                        lastActionTime = millis();
                    }
                } else {
                    // Set minute: rotary changes minute, SW finishes editing
                    if (input.rotaryDirection == 1 && lastDirection == 0) {
                        alarmMinute = (alarmMinute + 1) % 60;
                        lastActionTime = millis();
                    }
                    else if (input.rotaryDirection == -1 && lastDirection == 0) {
                        alarmMinute = (alarmMinute + 59) % 60;
                        lastActionTime = millis();
                    }
                    else if (input.swPressed) {
                        editingAlarm = false;
                        editingMinute = false;
                        alarmEditing = false;
                        alarmEnabled = true; // Automatically turn ON after setting time
                        lastActionTime = millis();
                    }
                }
                lastDirection = input.rotaryDirection;
            }
            
            if (shouldRender) {
                display.display();
            }
            break;
        }

        case MENU_CLOCK: {
            showClockPage();       // ✅ Draw the clock face
            showAlarmStatus(input);     // ✅ Overlay alarm icon/status with input (input processing in there)
            
            if (shouldRender) {
                display.display();  // ✅ Only render if throttle window elapsed
            }
            break;
        }

        case MENU_STOPWATCH: {
            // Stopwatch placeholder
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
            stopWatch(input); // Call the stopwatch loop function with input
            break;
        }

        case MENU_DISTANCE: {
            // === NEW: Distance Sensor Widget (Phase 2) ===
            if (distanceWidget) {
                distanceWidget->render();
            } else {
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(10, 25);
                display.println("Distance Sensor");
                display.setCursor(20, 35);
                display.println("Not Available");
                display.display();
            }
            break;
        }

        case MENU_ANGLE: {
            // === NEW: Angle/Level Sensor Widget (Phase 2) ===
            static unsigned long lastDebug = 0;
            if (millis() - lastDebug > 5000) {
                Serial.printf("🔍 UI Angle Check: Widget=%p, Sensor=%p\n", angleWidget, levelSensor);
                lastDebug = millis();
            }

            if (angleWidget && levelSensor) {
                angleWidget->render();
            } else {
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(10, 20);
                display.println("Level Sensor Error:");
                if (!angleWidget) display.println("- No Widget");
                if (!levelSensor) display.println("- No Sensor");
                display.setCursor(10, 45);
                display.println("Check Serial Log");
                display.display();
            }
            break;
        }
    }
}

