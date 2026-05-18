#include "mpu6050_angle.h"

LevelSensor::LevelSensor(Adafruit_MPU6050* mpu_ptr) : mpu(mpu_ptr) {
    // Constructor stores reference to existing MPU6050 instance
}

void LevelSensor::begin() {
    // MPU6050 is already initialized in main.cpp
    // Just reset filtered values
    filteredPitch = 0.0f;
    filteredRoll = 0.0f;
    lastUpdateTime = millis();
}

void LevelSensor::calibrate() {
    if (!mpu) return;

    float sumPitch = 0.0f, sumRoll = 0.0f;
    const int NUM_SAMPLES = 50;
    
    for (int i = 0; i < NUM_SAMPLES; i++) {
        sensors_event_t a, g, temp;
        if (mpu->getEvent(&a, &g, &temp)) {
            float ax = a.acceleration.x;
            float ay = a.acceleration.y;
            float az = a.acceleration.z;
            
            float pitch_rad = atan2(ax, sqrt(ay*ay + az*az));
            sumPitch += pitch_rad * 180.0f / M_PI;
            
            float roll_rad = atan2(ay, sqrt(ax*ax + az*az));
            sumRoll += roll_rad * 180.0f / M_PI;
        }
        delay(10);
    }
    
    calibOffsetPitch = sumPitch / NUM_SAMPLES;
    calibOffsetRoll = sumRoll / NUM_SAMPLES;
}

void LevelSensor::getAngles(float& pitch, float& roll) {
    if (!mpu) {
        pitch = 0; roll = 0;
        return;
    }

    unsigned long nowTime = millis();
    if (nowTime - lastUpdateTime < UPDATE_INTERVAL) {
        pitch = filteredPitch;
        roll = filteredRoll;
        return;
    }
    lastUpdateTime = nowTime;
    
    sensors_event_t a, g, temp;
    if (!mpu->getEvent(&a, &g, &temp)) {
        pitch = filteredPitch;
        roll = filteredRoll;
        return;
    }
    
    float ax = a.acceleration.x;
    float ay = a.acceleration.y;
    float az = a.acceleration.z;
    
    float magnitude = sqrt(ax*ax + ay*ay + az*az);
    if (magnitude > 0.1f) {
        float pitch_deg = (atan2(ax, sqrt(ay*ay + az*az)) * 180.0f / M_PI) - calibOffsetPitch;
        float roll_deg = (atan2(ay, sqrt(ax*ax + az*az)) * 180.0f / M_PI) - calibOffsetRoll;
        
        filteredPitch = ALPHA * pitch_deg + (1.0f - ALPHA) * filteredPitch;
        filteredRoll = ALPHA * roll_deg + (1.0f - ALPHA) * filteredRoll;
    }
    
    pitch = filteredPitch;
    roll = filteredRoll;
}

void LevelSensor::resetCalibration() {
    // Reset calibration to zero offsets
    calibOffsetPitch = 0.0f;
    calibOffsetRoll = 0.0f;
}
