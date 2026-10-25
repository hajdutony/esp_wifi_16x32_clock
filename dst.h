// dst.h - Daylight Saving Time handler for Europe/Bratislava (CET/CEST)
// EU DST rule: last Sunday of March 01:00 UTC → last Sunday of October 01:00 UTC

#ifndef DST_H
#define DST_H

#include <Arduino.h>
#include <time.h>

// Returns the current Europe/Bratislava UTC offset in seconds.
//  3600 = CET  (UTC+1, winter / standard time)
//  7200 = CEST (UTC+2, summer / daylight time)
int getDSTOffset(uint32_t utcEpoch);

// Convert UTC epoch to broken-down time (year, month, day, hour, minute, second)
// Used by main.ino to compute local time after applying DST offset
void epochToTime(uint32_t epoch, uint16_t &year, uint8_t &month, uint8_t &day,
                 uint8_t &hour, uint8_t &minute, uint8_t &second);

// Check if a UTC epoch falls within DST (returns true during CEST, false during CET)
bool isDST(uint32_t epoch);

#endif
