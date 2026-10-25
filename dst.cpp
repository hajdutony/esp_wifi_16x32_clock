// dst.cpp - Daylight Saving Time handler for Europe/Bratislava (CET/CEST)
// EU DST rule: last Sunday of March 01:00 UTC -> last Sunday of October 01:00 UTC
// Winter offset (CET):  3600 seconds (UTC+1)
// Summer offset (CEST): 7200 seconds (UTC+2)

#include "dst.h"

// ---- Internal helpers (definitions — must appear before callers) ----

static bool isLeapYear(uint16_t year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static uint8_t dayOfWeek(uint16_t year, uint8_t month, uint8_t day) {
  // Zeller's congruence: 0=Sunday, 1=Monday, ..., 6=Saturday
  uint16_t y = year;
  uint8_t m = month;
  if (m <= 2) { m += 12; y--; }
  int h = (day + (13 * (m + 1)) / 5 + y + (y / 4) - (y / 100) + (y / 400)) % 7;
  return (h + 6) % 7;
}

static uint8_t lastDayOfWeekOfMonth(uint16_t year, uint8_t month, uint8_t targetDow) {
  static const uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  uint8_t dim = daysInMonth[month - 1];
  if (month == 2 && isLeapYear(year)) dim = 29;
  for (uint8_t d = dim; d >= 1; d--) {
    if (dayOfWeek(year, month, d) == targetDow) return d;
  }
  return 1;
}

// ---- Public API ----

int getDSTOffset(uint32_t utcEpoch) {
  const int STANDARD_OFFSET = 3600;
  const int DAYLIGHT_OFFSET = 7200;
  if (isDST(utcEpoch)) return DAYLIGHT_OFFSET;
  return STANDARD_OFFSET;
}

void epochToTime(uint32_t epoch, uint16_t &year, uint8_t &month, uint8_t &day,
                 uint8_t &hour, uint8_t &minute, uint8_t &second) {
  uint32_t seconds = epoch % 60;
  uint32_t totalMinutes = epoch / 60;
  uint32_t minutes = totalMinutes % 60;
  uint32_t totalHours = totalMinutes / 60;
  uint32_t days = totalHours / 24;
  uint32_t remainingHours = totalHours % 24;
  uint32_t d = days;

  year = 1970;
  while (true) {
    uint16_t daysInYear = isLeapYear(year) ? 366 : 365;
    if (d < daysInYear) break;
    d -= daysInYear;
    year++;
  }

  static const uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  uint8_t m = 0;
  bool leap = isLeapYear(year);
  while (m < 12) {
    uint8_t dim = daysInMonth[m];
    if (m == 1 && leap) dim = 29;
    if (d < dim) break;
    d -= dim;
    m++;
  }

  month = m + 1;
  day = d + 1;
  hour = remainingHours;
  minute = minutes;
  second = seconds;
}

bool isDST(uint32_t epoch) {
  uint16_t year;
  uint8_t month, day, hour, minute, second;
  epochToTime(epoch, year, month, day, hour, minute, second);

  if (month < 3 || month > 10) return false;
  if (month > 3 && month < 10) return true;

  uint8_t lastSundayMarch = lastDayOfWeekOfMonth(year, 3, 0);
  uint8_t lastSundayOctober = lastDayOfWeekOfMonth(year, 10, 0);

  if (month == 3) {
    uint32_t totalDays = 0;
    for (uint16_t y = 1970; y < year; y++) {
      totalDays += isLeapYear(y) ? 366 : 365;
    }
    totalDays += 59;
    if (isLeapYear(year)) totalDays += 1;
    totalDays += (lastSundayMarch - 1);
    uint32_t transitionEpoch = totalDays * 86400UL + 1 * 3600UL;
    return epoch >= transitionEpoch;
  }

  uint32_t totalDays = 0;
  for (uint16_t y = 1970; y < year; y++) {
    totalDays += isLeapYear(y) ? 366 : 365;
  }
  static const uint32_t prefixDays[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273};
  totalDays += prefixDays[9];
  if (isLeapYear(year)) totalDays += 1;
  totalDays += (lastSundayOctober - 1);
  uint32_t transitionEpoch = totalDays * 86400UL + 1 * 3600UL;
  return epoch < transitionEpoch;
}
