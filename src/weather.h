#ifndef WEATHER_H
#pragma once
#define WEATHER_H

#include <string>
#include <arduino.h>

extern std::string weatherRegion;
extern const char* apiKey;  // ← Stores API key
extern String units;  // ← Stores units (metric/imperial)
extern int selectedCityIndex;
extern const char* cityList[];
extern const int cityCount;
extern std::string currentWeather; 
extern bool changeWeather;  // ← Flag to indicate if weather region is being changed

void changeWeatherRegion();
void fetchWeather();  // ← Add this
 // ← Stores API result
#endif
// End of weather.h