#include <gtest/gtest.h>

#include "TimeConstants.h"
#include "Utils.h"

// Basic epoch mapping under the standard MJD definition
TEST(UtilsDateTime, UnixEpochMJD) {
    double mjd = GetMJD(1970, 1, 1, 0, 0, 0);
    EXPECT_DOUBLE_EQ(MJD_UNIX_EPOCH, mjd);
}

TEST(UtilsDateTime, RoundTripNoon) {
    double mjd = GetMJD(1970, 1, 1, 12, 0, 0);
    Time t = GetTimeStructFromMJD(mjd);
    EXPECT_EQ(1970, t.year);
    EXPECT_EQ(1, t.month);
    EXPECT_EQ(1, t.day);
    EXPECT_EQ(12, t.hour);
    EXPECT_EQ(0, t.min);
    EXPECT_EQ(0, t.sec);
}

TEST(UtilsDateTime, LeapYear2000Feb29) {
    double mjd = GetMJD(2000, 2, 29, 0, 0, 0);
    Time t = GetTimeStructFromMJD(mjd);
    EXPECT_EQ(2000, t.year);
    EXPECT_EQ(2, t.month);
    EXPECT_EQ(29, t.day);
}

TEST(UtilsDateTime, ParseVariousPatterns) {
    // ISO date
    EXPECT_DOUBLE_EQ(GetMJD(2020, 1, 2), ParseDate("2020-01-02", ISOdate));
    // ISO date time
    EXPECT_DOUBLE_EQ(GetMJD(2020, 1, 2, 3, 4, 5), ParseDate("2020-01-02 03:04:05", ISOdateTime));
    // Dots and slashes
    EXPECT_DOUBLE_EQ(GetMJD(2020, 1, 2), ParseDate("02.01.2020", DD_MM_YYYY));
    EXPECT_DOUBLE_EQ(GetMJD(2020, 1, 2), ParseDate("02/01/2020", DD_MM_YYYY));
    // Compact
    EXPECT_DOUBLE_EQ(GetMJD(2020, 1, 2), ParseDate("20200102", YYYY_MM_DD));
    EXPECT_DOUBLE_EQ(GetMJD(2020, 1, 2, 3, 4, 5), ParseDate("20200102 030405", YYYY_MM_DD_hh_mm_ss));
    // ISO with T
    EXPECT_DOUBLE_EQ(GetMJD(2020, 1, 2, 3, 4, 5), ParseDate("2020-01-02T03:04:05", guess));
}

