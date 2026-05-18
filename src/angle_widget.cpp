#include "angle_widget.h"
#include <math.h>

AngleWidget::AngleWidget(LevelSensor* sensor, Adafruit_SSD1306* disp) 
    : levelSensor(sensor), display(disp) {
    // Constructor stores pointers to sensor and display
}

void AngleWidget::render() {
    if (!levelSensor) {
        display->clearDisplay();
        display->setTextSize(1);
        display->setCursor(10, 20);
        display->println("Angle Sensor");
        display->setCursor(20, 30);
        display->println("Disabled");
        display->display();
        return;
    }

    // Throttle rendering
    unsigned long now = millis();
    if (now - lastRenderTime < RENDER_INTERVAL) {
        return;
    }
    lastRenderTime = now;
    
    // Get current angles
    float pitch, roll;
    levelSensor->getAngles(pitch, roll);
    
    // Clear display area for widget
    display->fillRect(0, 0, 128, 64, SSD1306_BLACK);
    
    // Title
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(35, 2);
    display->print("LEVEL");
    
    // Draw bubble level indicator (visual center)
    drawBubbleLevel(64, 20, pitch, roll, 12);
    
    // Draw pitch angle (X-axis rotation)
    drawAngleText(10, 40, "X:", pitch);
    
    // Draw roll angle (Y-axis rotation)
    drawAngleText(70, 40, "Y:", roll);
    
    // Draw level/tilt indicator at bottom
    display->setTextSize(1);
    display->setCursor(5, 58);
    if (fabs(pitch) < 5 && fabs(roll) < 5) {
        display->print("LEVEL");
    } else if (fabs(pitch) > 30 || fabs(roll) > 30) {
        display->print("STEEP");
    } else {
        display->print("TILTED");
    }
    
    // Update display
    display->display();
}

void AngleWidget::drawBubbleLevel(int centerX, int centerY, float angleX, float angleY, int radius) {
    // Draw outer circle (level indicator frame)
    display->drawCircle(centerX, centerY, radius, SSD1306_WHITE);
    
    // Draw crosshairs (axes)
    display->drawLine(centerX - radius - 2, centerY, centerX + radius + 2, centerY, SSD1306_WHITE);
    display->drawLine(centerX, centerY - radius - 2, centerX, centerY + radius + 2, SSD1306_WHITE);
    
    // Calculate bubble position based on angles
    // Pitch (X) tilts the bubble left/right
    // Roll (Y) tilts the bubble up/down
    
    // Normalize angles to pixel range
    // Clamp angles to -45 to +45 degrees for better visualization
    float clampedPitch = constrain(angleX, -45, 45);
    float clampedRoll = constrain(angleY, -45, 45);
    
    // Convert to pixel offset (scale: 45 degrees = radius pixels)
    int bubbleX = centerX + (clampedPitch / 45.0f) * (radius - 2);
    int bubbleY = centerY + (clampedRoll / 45.0f) * (radius - 2);
    
    // Draw bubble (small circle)
    display->fillCircle(bubbleX, bubbleY, 2, SSD1306_WHITE);
}

void AngleWidget::drawAngleText(int x, int y, const char* label, float angle) {
    display->setTextSize(1);
    display->setCursor(x, y);
    display->print(label);
    
    // Format angle as fixed-width string
    char buffer[8];
    snprintf(buffer, sizeof(buffer), "%+6.1f", angle);
    display->print(buffer);
    display->print((char)247);  // Degree symbol (°)
}

void AngleWidget::getAngles(float& pitch, float& roll) {
    levelSensor->getAngles(pitch, roll);
}
