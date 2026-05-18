#include "distance_widget.h"

DistanceWidget::DistanceWidget(DistanceSensor* sensor, Adafruit_SSD1306* disp) 
    : distanceSensor(sensor), display(disp) {
    // Constructor stores pointers to sensor and display
}

void DistanceWidget::render() {
    // Throttle rendering
    unsigned long now = millis();
    if (now - lastRenderTime < RENDER_INTERVAL) {
        return;
    }
    lastRenderTime = now;
    
    // Get current distance
    uint16_t distance = distanceSensor->getDistance();
    
    // Clear display area for widget
    display->fillRect(0, 0, 128, 64, SSD1306_BLACK);
    
    // Title
    display->setTextSize(2);
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(10, 5);
    display->print("Distance");
    
    // Distance value in large text
    display->setTextSize(3);
    display->setCursor(20, 25);
    
    if (distance <= 100) {
        // Valid measurement
        display->print(distance);
        display->setTextSize(1);
        display->print("cm");
    } else {
        // Out of range
        display->print("---");
    }
    
    // Draw distance bars visualization
    drawDistanceBars(distance, 10, 48, 108, 10);
    
    // Draw range indicator
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(10, 60);
    display->print("0");
    display->setCursor(115, 60);
    display->print("1m");
    
    // Update display
    display->display();
}

void DistanceWidget::drawDistanceBars(uint16_t distance, int x, int y, int width, int height) {
    // Draw border
    display->drawRect(x, y, width, height, SSD1306_WHITE);
    
    if (distance > 100) {
        // No valid reading or out of range, show empty bar
        return;
    }
    
    // Scale distance to bar width (0-100cm maps to 0-width pixels)
    // Clamp to valid range
    uint16_t barLength = (distance * width) / 100;
    if (barLength > width) barLength = width;
    
    // Draw filled bar to indicate distance
    if (distance < 20) {
        display->fillRect(x + 2, y + 2, barLength - 4, height - 4, SSD1306_WHITE);  // Very close
    } else if (distance < 60) {
        display->fillRect(x + 2, y + 2, barLength - 4, height - 4, SSD1306_WHITE);  // Medium
    } else {
        display->fillRect(x + 2, y + 2, barLength - 4, height - 4, SSD1306_WHITE);  // Far (within 1m)
    }
}

uint16_t DistanceWidget::getDistance() {
    return distanceSensor->getDistance();
}
