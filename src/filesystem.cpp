#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include "filesystem.h"

// ============================================================================
// FILESYSTEM UTILITIES FOR ESP32
// ============================================================================
// Centralized LittleFS management for animations and data files

/**
 * Initialize LittleFS filesystem
 * Automatically formats if mount fails (dev mode)
 */
bool initFilesystem() {
  if (!LittleFS.begin(true)) { // true = auto-format if mount fails
    Serial.println("❌ LittleFS mount failed!");
    return false;
  }

  Serial.println("✅ LittleFS filesystem initialized");
  return true;
}

/**
 * List files in a directory
 * Useful for enumeration and debugging
 */
void listFilesInDirectory(const char* path, int numSpaces) {
  File root = LittleFS.open(path);
  if (!root || !root.isDirectory()) {
    return;
  }

  File file = root.openNextFile();
  while (file) {
    for (int i = 0; i < numSpaces; i++) Serial.print("  ");
    Serial.printf("• %s", file.name());
    
    if (file.isDirectory()) {
      Serial.println(" (dir)");
      listFilesInDirectory(file.path(), numSpaces + 1);
    } else {
      Serial.printf(" (%d bytes)\n", (int)file.size());
    }
    file = root.openNextFile();
  }
}

/**
 * Check if a file exists
 */
bool fileExists(const char* filePath) {
  return LittleFS.exists(filePath);
}

/**
 * Get file size in bytes
 */
size_t getFileSize(const char* filePath) {
  File f = LittleFS.open(filePath, "r");
  if (!f) return 0;
  size_t size = f.size();
  f.close();
  return size;
}

/**
 * Read entire file into buffer (for small files)
 * Returns bytes read, or 0 if failed
 */
size_t readFileToBuffer(const char* filePath, uint8_t* buffer, size_t maxSize) {
  File f = LittleFS.open(filePath, "r");
  if (!f) {
    Serial.printf("❌ Cannot open file: %s\n", filePath);
    return 0;
  }

  if (f.size() > maxSize) {
    Serial.printf("⚠️ File too large (%d > %d bytes)\n", (int)f.size(), (int)maxSize);
    f.close();
    return 0;
  }

  size_t bytesRead = f.read(buffer, maxSize);
  f.close();
  return bytesRead;
}

/**
 * Delete a file
 */
bool deleteFile(const char* filePath) {
  if (!LittleFS.remove(filePath)) {
    Serial.printf("❌ Failed to delete: %s\n", filePath);
    return false;
  }
  Serial.printf("✅ Deleted: %s\n", filePath);
  return true;
}

/**
 * Get LittleFS usage statistics
 */
void printFilesystemStats() {
  size_t totalBytes = LittleFS.totalBytes();
  size_t usedBytes = LittleFS.usedBytes();
  size_t freeBytes = totalBytes - usedBytes;

  Serial.printf("\n📊 LittleFS Statistics:\n");
  Serial.printf("  Total: %d KB\n", (int)(totalBytes / 1024));
  Serial.printf("  Used:  %d KB (%.1f%%)\n", (int)(usedBytes / 1024), 
                (100.0f * usedBytes) / totalBytes);
  Serial.printf("  Free:  %d KB\n", (int)(freeBytes / 1024));
  Serial.println();
}

/**
 * Validate animation file format
 * Checks: file exists, readable, has valid header
 */
bool validateAnimationFile(const char* filePath) {
  File f = LittleFS.open(filePath, "r");
  if (!f) {
    Serial.printf("❌ File not found: %s\n", filePath);
    return false;
  }

  // Read frame count header
  uint8_t lo = f.read();
  uint8_t hi = f.read();
  f.close();

  if (lo == 255 && hi == 255) { // Check for EOF/Failure
    Serial.printf("❌ File too small or corrupted: %s\n", filePath);
    return false;
  }

  uint16_t frameCount = (hi << 8) | lo;
  if (frameCount == 0 || frameCount > 1000) { // Sanity check
    Serial.printf("⚠️ Suspicious frame count (%d) in %s\n", frameCount, filePath);
    return false;
  }

  Serial.printf("✅ Animation file valid: %s (%d frames)\n", filePath, (int)frameCount);
  return true;
}

