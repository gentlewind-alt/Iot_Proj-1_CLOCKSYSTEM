#ifndef MPU6050_ANGLE_H
#define MPU6050_ANGLE_H

#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>

// MPU6050 tilt angle measurement
// Calculates pitch (X-axis) and roll (Y-axis) from accelerometer

class LevelSensor {
private:
    Adafruit_MPU6050* mpu;
    
    // Angle filtering (exponential moving average / low-pass filter)
    static constexpr float ALPHA = 0.2f;  // Filter coefficient (0.0-1.0, higher = more responsive)
    float filteredPitch = 0.0f;
    float filteredRoll = 0.0f;
    
    // Calibration offsets (set on startup)
    float calibOffsetPitch = 0.0f;
    float calibOffsetRoll = 0.0f;
    
    // Update throttling
    unsigned long lastUpdateTime = 0;
    static constexpr unsigned long UPDATE_INTERVAL = 50;  // Update every 50ms
    
    // Sensor data
    sensors_event_t accel;
    
public:
    LevelSensor(Adafruit_MPU6050* mpu_ptr);
    
    // Initialize with existing MPU6050 instance
    void begin();
    
    // Calibrate offsets (call at startup)
    void calibrate();
    
    // Get pitch angle in degrees (-180 to +180)
    float getPitch();
    
    // Get roll angle in degrees (-180 to +180)
    float getRoll();
    
    // Get both angles
    void getAngles(float& pitch, float& roll);
    
    // Reset calibration to zero offsets
    void resetCalibration();
    
private:
    // Convert accelerometer values to angle
    float accelToAngle(float acc_x, float acc_y, float acc_z);
};

#endif // MPU6050_ANGLE_H
