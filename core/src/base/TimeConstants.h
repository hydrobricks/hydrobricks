#ifndef HYDROBRICKS_TIME_CONSTANTS_H
#define HYDROBRICKS_TIME_CONSTANTS_H

/**
 * Centralized time/date constants used across the codebase.
 *
 * References:
 * - IAU/USNO: Modified Julian Date (MJD) is defined as MJD = JD - 2400000.5
 *   https://aa.usno.navy.mil/faq/julian-date
 */
namespace TimeConstants {

// Formal definition used in Hydrobricks
static constexpr double JD_TO_MJD_OFFSET = 2400000.5;

// Seconds per day
static constexpr double SECONDS_PER_DAY_D = 86400.0;
static constexpr long long SECONDS_PER_DAY_LL = 86400LL;

// MJD value of Unix epoch (1970-01-01 00:00:00 UTC) with the standard definition
// JD(1970-01-01 00:00:00) = 2440587.5; 2440587.5 - 2400000.5 = 40587.0
static constexpr double MJD_UNIX_EPOCH = 40587.0;

}  // namespace TimeConstants

using TimeConstants::JD_TO_MJD_OFFSET;
using TimeConstants::MJD_UNIX_EPOCH;
using TimeConstants::SECONDS_PER_DAY_D;
using TimeConstants::SECONDS_PER_DAY_LL;

#endif  // HYDROBRICKS_TIME_CONSTANTS_H
