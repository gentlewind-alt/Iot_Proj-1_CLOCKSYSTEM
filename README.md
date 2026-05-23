# IoT Smart Alarm Clock (ESP32-C3)

A high-performance, feature-rich smart alarm clock built on the **ESP32-C3**, featuring a modular sensor architecture, custom streaming animation system, and sophisticated power/memory optimizations.

![Project Status](https://img.shields.io/badge/status-completed-success)
![Platform](https://img.shields.io/badge/platform-ESP32--C3-blue)
![Framework](https://img.shields.io/badge/framework-Arduino/PlatformIO-orange)

## 🚀 Key Features

- **Precision Timekeeping**: Dual-source time management using **NTP Synchronization** (WiFi) and a **DS1307 Hardware RTC** for reliable offline operation.
- **Custom Animation Engine**: A memory-efficient `.bin` streaming system that plays high-quality animations from **LittleFS**, bypassing ESP32-C3's RAM constraints.
- **Sensor-Driven Interaction**:
    - **Motion-Activated UX**: PIR sensor wakes the clock from idle mode and can dismiss alarms.
    - **Hand Gesture Control**: HC-SR04 ultrasonic sensor for touchless snooze and distance-based UI feedback.
    - **Level/Angle Widget**: Real-time orientation tracking using an **MPU6050** IMU.
- **Intelligent UI**:
    - Multi-page menu system driven by a **Rotary Encoder**.
    - **NeoPixel Status LED**: Context-aware RGB feedback (Rainbow idle, Red alert, Blue alarm-enabled).
    - **Musical Alarms**: Polyphonic-style chord alerts (Cmaj7, G, Am7, Fmaj7) via a passive buzzer.
- **Efficiency First**: Deeply optimized codebase with throttled sensor polling, rendering caches, and custom memory management.

---

## 🛠️ Hardware Architecture

| Component | Purpose | Interface |
|-----------|---------|-----------|
| **ESP32-C3** | Main Microcontroller (160MHz, 320KB RAM) | - |
| **SSD1306 OLED** | 128x64 Primary Display | I2C |
| **DS1307 RTC** | Battery-backed Real-Time Clock | I2C |
| **MPU6050** | 6-Axis Accelerometer/Gyroscope | I2C |
| **HC-SR04** | Ultrasonic Distance Sensor | GPIO (Trig/Echo) |
| **PIR Sensor** | Passive Infrared Motion Detection | GPIO |
| **Rotary Encoder** | Menu Navigation & Input | GPIO (Interrupts) |
| **NeoPixel (WS2812)** | Multi-mode Status Indicator | GPIO |
| **Passive Buzzer** | Chord-based Alarm Tones | PWM/LEDC |

---

## 💻 Technical Highlights

### 1. The "RAM Crisis" & Memory Optimization
The ESP32-C3 features 320KB of DRAM, which is easily exhausted by the standard WiFi/BT stack. 
- **BSS Reduction**: Successfully reduced BSS allocation from **935KB to 319KB** by implementing a modular feature toggle system and stripping unneeded static buffers.
- **Custom Partitioning**: Optimized `partitions.csv` to balance OTA (Over-the-Air) updates (2.26MB) and LittleFS asset storage (1.69MB).

### 2. Streaming Animation System
Replaced legacy PROGMEM-based animation (which caused flash overflow) with a custom `.bin` format.
- **Streaming**: Frames are read one-by-one from LittleFS into a small buffer, allowing for 50+ high-quality animations without impacting RAM.
- **Format**: Implemented a custom packing format with 16-byte headers for magic byte verification and frame metadata.

### 3. CPU & Bus Throttling
To ensure a smooth UI at 60 FPS while managing multiple sensors:
- **Sensor Throttling**: Reduced HC-SR04 polling from 60Hz to 10Hz with a fast 3-sample median filter.
- **UI Caching**: Implemented a 500ms cache for expensive `snprintf` time-formatting operations, reducing CPU overhead by ~40%.
- **Display Throttling**: Capped I2C display updates to 20 FPS to prevent bus congestion while maintaining visual fluidity.

---

## 📂 Project Structure

```text
├── include/              # Header files (Modules: Sensors, Clock, UI)
├── src/                  # Implementation files
│   ├── animation.cpp     # .bin Streaming Engine
│   ├── clock.cpp         # NTP/RTC Time Logic
│   ├── interface.cpp     # Menu & Widget System
│   └── main.cpp          # System Orchestration
├── data/                 # LittleFS Assets (.bin animations)
├── platformio.ini        # Build configurations & optimizations
└── partitions.csv        # Custom memory mapping
```

---

## 🔧 Installation & Setup

1. **Prerequisites**: Install [PlatformIO](https://platformio.org/).
2. **Clone the Repo**:
   ```bash
   git clone https://github.com/yourusername/iot-smart-clock.git
   ```
3. **Hardware Wiring**: Refer to `diagram.json` or the pin definitions in `include/interface.h`.
4. **Flash Assets**: 
   ```bash
   # Upload the filesystem data (animations)
   pio run --target uploadfs
   ```
5. **Build & Flash**:
   ```bash
   pio run --target upload
   ```

---

## 📝 License
This project is open-source and available under the MIT License.

---

*Developed as part of an advanced IoT prototyping project, focusing on resource-constrained embedded systems.*
