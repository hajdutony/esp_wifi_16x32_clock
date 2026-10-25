// sun.h - civil dawn/dusk calculation (no Arduino dependencies, testable on a PC)

#ifndef SUN_H
#define SUN_H

#include <time.h>

// Civil dawn and dusk (sun 6 degrees below the horizon) for a calendar date
// at the given location (latitude north +, longitude east +), returned as
// UTC epochs. Convert them with localtime_r() to get local time with DST.
// Returns false if the sun does not cross -6 degrees that day (polar regions).
bool civilTwilight(int year, int month, int day, double lat, double lon,
                   time_t &dawn, time_t &dusk);

#endif
