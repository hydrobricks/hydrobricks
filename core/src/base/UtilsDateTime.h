#ifndef HYDROBRICKS_UTILS_DATETIME_H
#define HYDROBRICKS_UTILS_DATETIME_H

#include <chrono>
#include <string>

#include "Enums.h"
#include "Includes.h"

/**
 * Date/time conversion utilities centralizing MJD/JD/chrono operations.
 *
 * Implementation notes:
 * - Uses C++20 chrono (sys_days, from_stream) for robust parsing.
 * - Uses standard MJD definition (JD - 2400000.5).
 * - All operations assume UTC; no time zone or DST adjustments are applied.
 */
class UtilsDateTime {
  public:
    // Convert a structured date-time to MJD (double days).
    static double ToMJD(int year, int month, int day, int hour = 0, int minute = 0, int second = 0);

    // Convert MJD to structured time.
    static Time FromMJD(double mjd);

    // Parse date/time string according to TimeFormat; throws InputError on failure.
    static double ParseToMJD(const std::string& dateStr, TimeFormat format);

  private:
    // Helpers
    static std::chrono::sys_days ToSysDays(int year, int month, int day);
    static void ValidateHMS(int hour, int minute, int second);
};

#endif  // HYDROBRICKS_UTILS_DATETIME_H
