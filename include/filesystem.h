#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <Arduino.h>

bool initFilesystem();
void listFilesInDirectory(const char* path, int numSpaces = 0);
bool fileExists(const char* filePath);
size_t getFileSize(const char* filePath);
size_t readFileToBuffer(const char* filePath, uint8_t* buffer, size_t maxSize);
bool deleteFile(const char* filePath);
void printFilesystemStats();
bool validateAnimationFile(const char* filePath);

#endif // FILESYSTEM_H
