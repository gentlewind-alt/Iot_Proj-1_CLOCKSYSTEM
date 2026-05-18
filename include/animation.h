#ifndef ANIMATION_H
#define ANIMATION_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <FS.h>
#include <LittleFS.h>

/**
 * ANIMATION PLAYER - .bin File Streaming Renderer
 * 
 * Format (.bin):
 *   - Bytes 0-1:   uint16_t frameCount (little-endian)
 *   - Bytes 2+:    [Per frame]
 *     - uint16_t frameSize
 *     - uint8_t frameData[frameSize]
 * 
 * Memory Strategy:
 *   - Single frame buffer (~1024 bytes)
 *   - Streams frames from SPIFFS/LittleFS
 *   - No PROGMEM overhead for animations
 */

class AnimationPlayer {
private:
  File animFile;
  uint16_t totalFrames;
  uint16_t currentFrame;
  uint8_t frameBuffer[1024];      // Max 128x64 = 1024 bytes
  uint16_t currentFrameSize;
  uint16_t bytesPerFrame;         // For fixed-size legacy BINPACK
  uint16_t animWidth, animHeight; 
  bool isLegacyFormat;
  
  bool isPlaying;
  bool loopEnabled;
  unsigned long lastFrameTime;
  uint16_t frameDelayMs;

  Adafruit_SSD1306* display;
  char currentPath[64];

public:
  AnimationPlayer(Adafruit_SSD1306* disp);
  ~AnimationPlayer();

  // Load animation from .bin file
  bool loadAnimation(const char* filePath);
  void closeAnimation();

  // Playback control
  void play(uint16_t delayMs = 80, bool loop = true);
  void stop();
  bool isRunning() const { return isPlaying; }

  // Frame management
  bool nextFrame();
  bool renderFrame();
  void reset();

  // Info
  uint16_t getFrameCount() const { return totalFrames; }
  uint16_t getCurrentFrame() const { return currentFrame; }
};

// Global animation player instance
extern AnimationPlayer* globalAnimPlayer;

// Initialize animation system
bool initAnimationSystem(Adafruit_SSD1306* display);

// Convenience blocking player
bool playAnimationBlocking(const char* filePath, uint16_t delayMs = 80, bool loop = true);

// Play a random animation from a directory (and subdirectories)
bool playRandomAnimation(const char* directory = "/idle", uint16_t delayMs = 80, bool loop = false);

// Play all animations in a directory in sequence
bool playAnimationsInSequence(const char* directory, uint16_t delayMs = 80);

#endif // ANIMATION_H
