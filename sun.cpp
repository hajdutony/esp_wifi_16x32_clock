// sun.cpp - civil dawn/dusk calculation
// Sunrise/sunset algorithm from the Almanac for Computers (US Naval
// Observatory, 1990), accurate to about 1-2 minutes at mid latitudes.

#include "sun.h"
#include <math.h>

static const double DEG = M_PI / 180.0;
static const double CIVIL_ZENITH = 96.0;        // 90 + 6 degrees below the horizon

static double normalize(double v, double range) {
  v = fmod(v, range);
  return v < 0 ? v + range : v;
}

// Days since 1970-01-01 for a calendar date (proleptic Gregorian)
static long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  long era = (y >= 0 ? y : y - 399) / 400;
  long yoe = y - era * 400;
  long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

// UTC hour (0-24) of a sun event on day-of-year n, or -1 if it doesn't happen
static double eventUtcHour(int n, double lat, double lon, bool rising) {
  double lngHour = lon / 15.0;
  double t = n + ((rising ? 6.0 : 18.0) - lngHour) / 24.0;

  double M = 0.9856 * t - 3.289;                // Sun's mean anomaly
  double L = normalize(M + 1.916 * sin(M * DEG) + 0.020 * sin(2 * M * DEG) + 282.634, 360.0);

  double RA = normalize(atan(0.91764 * tan(L * DEG)) / DEG, 360.0);
  RA += floor(L / 90.0) * 90.0 - floor(RA / 90.0) * 90.0;  // Same quadrant as L
  RA /= 15.0;

  double sinDec = 0.39782 * sin(L * DEG);
  double cosDec = cos(asin(sinDec));
  double cosH = (cos(CIVIL_ZENITH * DEG) - sinDec * sin(lat * DEG)) / (cosDec * cos(lat * DEG));
  if (cosH > 1.0 || cosH < -1.0) return -1;     // Never gets that dark / that light

  double H = acos(cosH) / DEG;
  if (rising) H = 360.0 - H;
  H /= 15.0;

  double T = H + RA - 0.06571 * t - 6.622;      // Local mean time of the event
  return normalize(T - lngHour, 24.0);
}

bool civilTwilight(int year, int month, int day, double lat, double lon,
                   time_t &dawn, time_t &dusk) {
  long days = daysFromCivil(year, month, day);
  int n = days - daysFromCivil(year, 1, 1) + 1;   // Day of year, 1-366
  double rise = eventUtcHour(n, lat, lon, true);
  double set = eventUtcHour(n, lat, lon, false);
  if (rise < 0 || set < 0) return false;

  // Event hours are UTC on (approximately) this date. For locations far east
  // or west the UTC date can differ, so keep each event within +-12 h of
  // local noon at this longitude.
  time_t midnightUtc = (time_t)days * 86400;
  double noonUtcHour = 12.0 - lon / 15.0;
  if (rise - noonUtcHour > 12) rise -= 24; else if (noonUtcHour - rise > 12) rise += 24;
  if (set - noonUtcHour > 12) set -= 24; else if (noonUtcHour - set > 12) set += 24;

  dawn = midnightUtc + (time_t)(rise * 3600.0 + 0.5);
  dusk = midnightUtc + (time_t)(set * 3600.0 + 0.5);
  return true;
}
