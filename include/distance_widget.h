#ifndef DISTANCE_WIDGET_H
#define DISTANCE_WIDGET_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "hcsr04.h"

// Distance sensor display widget
// Shows distance measurement in cm with visual indicators

class DistanceWidget {
private:
    DistanceSensor* distanceSensor;
    Adafruit_SSD1306* display;
    uint16_t lastDisplay = 0;
    unsigned long lastRenderTime = 0;
    static constexpr unsigned long RENDER_INTERVAL = 100;  // Update every 100ms
    
public:
    DistanceWidget(DistanceSensor* sensor, Adafruit_SSD1306* disp);
    
    // Render widget on display (call each frame)
    void render();
    
    // Get current distance value
    uint16_t getDistance();
    
private:
    // Helper to draw distance bars for visualization
    void drawDistanceBars(uint16_t distance, int x, int y, int width, int height);
};

#endif // DISTANCE_WIDGET_H
