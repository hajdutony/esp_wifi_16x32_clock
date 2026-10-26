// weather.h - Open-Meteo response parsing (no Arduino dependencies, testable on a PC)

#ifndef WEATHER_H
#define WEATHER_H

#include "weather_icons.h"

// Precipitation in a 15-minute interval that counts as rain coming (mm)
#define RAIN_SOON_MM 0.1

struct Weather {
  int temperature;                              // Rounded, deg C
  int code;                                     // WMO weather code
  bool rainSoon;                                // Precipitation expected within the next hour
};

// Path and query for api.open-meteo.com: current temperature and weather
// code, plus precipitation for the next four 15-minute intervals
void weatherRequestPath(char *buf, int size, double lat, double lon);

// Parse the JSON response. Returns false if a value is missing.
bool parseWeather(const char *json, Weather &out);

// Icon and English description for a WMO weather code
WeatherIcon weatherIcon(int code);
const char *weatherDescription(int code);

#endif
