#include "animation.h"
#include <LittleFS.h>
#include <vector>
#include <algorithm>

AnimationPlayer* globalAnimPlayer = nullptr;

AnimationPlayer::AnimationPlayer(Adafruit_SSD1306* disp) 
    : display(disp), totalFrames(0), currentFrame(0), isPlaying(false), 
      loopEnabled(false), lastFrameTime(0), frameDelayMs(80),
      bytesPerFrame(1024), animWidth(128), animHeight(64), isLegacyFormat(false) {
    currentPath[0] = '\0';
}

AnimationPlayer::~AnimationPlayer() {
    closeAnimation();
}

bool AnimationPlayer::loadAnimation(const char* filePath) {
    closeAnimation();

    animFile = LittleFS.open(filePath, "r");
    if (!animFile) {
        Serial.printf("❌ Failed to open animation: %s\n", filePath);
        return false;
    }

    char magic[8];
    if (animFile.read((uint8_t*)magic, 8) == 8 && strncmp(magic, "BINPACK", 7) == 0) {
        // --- Standard BINPACK Format (Legacy) ---
        isLegacyFormat = true;
        uint16_t w, h;
        uint32_t count;
        if (animFile.read((uint8_t*)&w, 2) != 2 || 
            animFile.read((uint8_t*)&h, 2) != 2 || 
            animFile.read((uint8_t*)&count, 4) != 4) {
            Serial.println("❌ Invalid BINPACK header");
            animFile.close();
            return false;
        }
        animWidth = w;
        animHeight = h;
        totalFrames = (uint16_t)count;
        bytesPerFrame = (w * h) / 8;
        Serial.printf("✅ Loaded BINPACK: %dx%d, %d frames\n", animWidth, animHeight, totalFrames);
    } else {
        // --- Simple Format ---
        isLegacyFormat = false;
        animWidth = 128;
        animHeight = 64;
        animFile.seek(0); // Go back to start
        if (animFile.read((uint8_t*)&totalFrames, 2) != 2) {
            Serial.println("❌ Failed to read frame count");
            animFile.close();
            return false;
        }
        Serial.printf("✅ Loaded Simple: %d frames\n", totalFrames);
    }

    strncpy(currentPath, filePath, sizeof(currentPath) - 1);
    currentFrame = 0;
    return true;
}

void AnimationPlayer::closeAnimation() {
    if (animFile) {
        animFile.close();
    }
    isPlaying = false;
    totalFrames = 0;
    currentFrame = 0;
}

void AnimationPlayer::play(uint16_t delayMs, bool loop) {
    if (totalFrames == 0) return;
    frameDelayMs = delayMs;
    loopEnabled = loop;
    isPlaying = true;
    lastFrameTime = millis();
    
    // Render first frame immediately
    renderFrame();
}

void AnimationPlayer::stop() {
    isPlaying = false;
}

void AnimationPlayer::reset() {
    if (animFile) {
        if (isLegacyFormat) {
            animFile.seek(16); // Skip 16-byte BINPACK header
        } else {
            animFile.seek(2); // Skip 2-byte frame count
        }
    }
    currentFrame = 0;
}

bool AnimationPlayer::nextFrame() {
    if (!isPlaying || totalFrames == 0) return false;

    if (millis() - lastFrameTime >= frameDelayMs) {
        currentFrame++;
        if (currentFrame >= totalFrames) {
            if (loopEnabled) {
                reset();
            } else {
                isPlaying = false;
                return false;
            }
        }
        
        if (renderFrame()) {
            lastFrameTime = millis();
            return true;
        }
    }
    return false;
}

bool AnimationPlayer::renderFrame() {
    if (!animFile) return false;

    // Read frame size
    uint16_t frameSize;
    if (isLegacyFormat) {
        frameSize = bytesPerFrame;
    } else {
        // Read frame size (2 bytes)
        if (animFile.read((uint8_t*)&frameSize, 2) != 2) {
            if (loopEnabled) {
                reset();
                return renderFrame();
            }
            return false;
        }
    }

    if (frameSize > sizeof(frameBuffer)) {
        Serial.printf("❌ Frame size %d exceeds buffer\n", frameSize);
        return false;
    }

    // Read frame data
    if (animFile.read(frameBuffer, frameSize) != frameSize) {
        return false;
    }

    display->clearDisplay();
    display->drawBitmap(0, 0, frameBuffer, animWidth, animHeight, SSD1306_WHITE);
    display->display();

    return true;
}

bool initAnimationSystem(Adafruit_SSD1306* display) {
    if (globalAnimPlayer) delete globalAnimPlayer;
    globalAnimPlayer = new AnimationPlayer(display);
    return true;
}

bool playAnimationBlocking(const char* filePath, uint16_t delayMs, bool loop) {
    if (!globalAnimPlayer) return false;
    
    if (!globalAnimPlayer->loadAnimation(filePath)) return false;
    
    globalAnimPlayer->play(delayMs, loop);
    
    while (globalAnimPlayer->isRunning()) {
        globalAnimPlayer->nextFrame();
        delay(1);
        yield();
    }
    
    return true;
}

static void collectBinFiles(const char* directory, std::vector<String>& files) {
    File root = LittleFS.open(directory);
    if (!root || !root.isDirectory()) return;

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            collectBinFiles(file.path(), files);
        } else {
            String fileName = String(file.name());
            if (fileName.endsWith(".bin")) {
                files.push_back(String(file.path()));
            }
        }
        file = root.openNextFile();
    }
}

bool playRandomAnimation(const char* directory, uint16_t delayMs, bool loop) {
    if (!globalAnimPlayer) return false;

    static unsigned long lastErrorTime = 0;
    std::vector<String> files;
    collectBinFiles(directory, files);

    if (files.empty()) {
        if (millis() - lastErrorTime > 10000) { // Only log every 10 seconds
            Serial.printf("⚠️ No .bin files found in %s. Please upload animations to LittleFS.\n", directory);
            lastErrorTime = millis();
        }
        return false;
    }

    int r = random(files.size());
    Serial.printf("🎲 Playing random animation: %s\n", files[r].c_str());
    return playAnimationBlocking(files[r].c_str(), delayMs, loop);
}

bool playAnimationsInSequence(const char* directory, uint16_t delayMs) {
    if (!globalAnimPlayer) return false;

    std::vector<String> files;
    collectBinFiles(directory, files);

    if (files.empty()) return false;

    // Sort files alphabetically
    std::sort(files.begin(), files.end());

    Serial.printf("📑 Playing %d animations in sequence from %s\n", (int)files.size(), directory);
    for (const String& path : files) {
        if (!playAnimationBlocking(path.c_str(), delayMs, false)) {
            return false;
        }
    }
    return true;
}
