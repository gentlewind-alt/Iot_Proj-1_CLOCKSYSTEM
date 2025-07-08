#ifndef ENV_API
#endif

const char* apiKey = ENV_API;
#include "interface.h"// For JSON parsing

std::string currentWeather = "Loading...";
static bool changeWeather = false;
static int lastRotaryDirection = 0;
// List of up to 5 city names or coordinates
const char* cityList[] = {
  "Tokyo", "Delhi", "London", "Paris", "Colombia",
  "Berlin", "Moscow", "Seoul", "Dhenkanal", "Mumbai"
};
const int cityCount = sizeof(cityList) / sizeof(cityList[0]);

int selectedCityIndex = 0;
std::string weatherRegion = cityList[selectedCityIndex];

void changeWeatherRegion() {
  InputState weatherChangeinput;
  if (changeWeather ){
  if  (weatherChangeinput.rotaryDirection == 1 && lastRotaryDirection == 0) {
      selectedCityIndex = (selectedCityIndex + 1) % cityCount; 
    }    
  } else if (changeWeather && weatherChangeinput.rotaryDirection == -1 && lastRotaryDirection == 0) {
      selectedCityIndex = (selectedCityIndex - 1 + cityCount) % cityCount;
  }
  lastRotaryDirection = weatherChangeinput.rotaryDirection;
  weatherRegion = cityList[selectedCityIndex];
  fetchWeather();  // auto-update when changed
}

void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    currentWeather = "No WiFi";
    return;
  }

  String url = "http://api.openweathermap.org/data/2.5/weather?q=";
  url += weatherRegion.c_str();
  url += "&appid=";
  url += apiKey;
  url += "&units=metric";

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      float temp = doc["main"]["temp"];
      char tempStr[16];
      snprintf(tempStr, sizeof(tempStr), "%.2f", temp);
      currentWeather = weatherRegion + ": " + tempStr + " C";
    } else {
      currentWeather = "Parse Err";
    }
  } else {
    currentWeather = "HTTP Error";
  }

  http.end();
}