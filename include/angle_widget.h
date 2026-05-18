#ifndef ANGLE_WIDGET_H
#define ANGLE_WIDGET_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "mpu6050_angle.h"

// Angle/level sensor display widget
// Shows pitch (X) and roll (Y) angles in degrees with visual level indicators

class AngleWidget {
private:
    LevelSensor* levelSensor;
    Adafruit_SSD1306* display;
    unsigned long lastRenderTime = 0;
    static constexpr unsigned long RENDER_INTERVAL = 100;  // Update every 100ms
    
public:
    AngleWidget(LevelSensor* sensor, Adafruit_SSD1306* disp);
    
    // Render widget on display (call each frame)
    void render();
    
    // Get current angles
    void getAngles(float& pitch, float& roll);
    
private:
    // Helper to draw bubble level indicator
    void drawBubbleLevel(int centerX, int centerY, float angleX, float angleY, int radius);
    
    // Helper to draw angle text
    void drawAngleText(int x, int y, const char* label, float angle);
};

#endif // ANGLE_WIDGET_H
