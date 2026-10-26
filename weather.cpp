// weather.cpp - Open-Meteo response parsing
// The response is small and fixed in shape, so a few string searches are
// enough and no JSON library is needed.

#include "weather.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void weatherRequestPath(char *buf, int size, double lat, double lon) {
  snprintf(buf, size,
           "/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,weather_code"
           "&minutely_15=precipitation&forecast_minutely_15=4&timezone=UTC",
           lat, lon);
}

// Number after "key": inside the object that starts at from
static bool findNumber(const char *from, const char *key, double &value) {
  const char *p = strstr(from, key);
  if (!p) return false;
  p += strlen(key);
  char *end;
  value = strtod(p, &end);
  return end != p;
}

bool parseWeather(const char *json, Weather &out) {
  // "current": (not "current_units":) holds the values
  const char *current = strstr(json, "\"current\":{");
  if (!current) return false;
  double temp, code;
  if (!findNumber(current, "\"temperature_2m\":", temp)) return false;
  if (!findNumber(current, "\"weather_code\":", code)) return false;
  out.temperature = (int)lround(temp);
  out.code = (int)code;

  // "precipitation":[0.40,0.10,...] in "minutely_15":
  out.rainSoon = false;
  const char *minutely = strstr(json, "\"minutely_15\":{");
  const char *p = minutely ? strstr(minutely, "\"precipitation\":[") : nullptr;
  if (!p) return false;
  p += strlen("\"precipitation\":[");
  while (*p && *p != ']') {
    char *end;
    double mm = strtod(p, &end);
    if (end == p) {                             // null (no data) or junk: skip it
      while (*p && *p != ',' && *p != ']') p++;
    } else {
      if (mm >= RAIN_SOON_MM) out.rainSoon = true;
      p = end;
    }
    if (*p == ',') p++;
  }
  return true;
}

// WMO weather interpretation codes, as used by Open-Meteo
WeatherIcon weatherIcon(int code) {
  if (code == 0) return ICON_SUN;
  if (code <= 2) return ICON_PARTLY;
  if (code == 3) return ICON_CLOUD;
  if (code == 45 || code == 48) return ICON_FOG;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return ICON_SNOW;
  if (code >= 95) return ICON_STORM;
  if (code >= 51 && code <= 82) return ICON_RAIN;   // Drizzle, rain, freezing rain, showers
  return ICON_CLOUD;
}

const char *weatherDescription(int code) {
  switch (weatherIcon(code)) {
    case ICON_SUN: return "clear";
    case ICON_PARTLY: return "partly cloudy";
    case ICON_CLOUD: return "cloudy";
    case ICON_RAIN: return "rain";
    case ICON_SNOW: return "snow";
    case ICON_STORM: return "thunderstorm";
    case ICON_FOG: return "fog";
    default: return "";
  }
}
