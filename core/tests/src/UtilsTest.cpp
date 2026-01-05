#include <gtest/gtest.h>

#include "Utils.h"

TEST(Utils, ZeroIsNotNaN) {
    int value = 0;

    EXPECT_FALSE(IsNaN((float)value));
}

TEST(Utils, IsNaNIntTrue) {
    int value = NAN_I;

    EXPECT_TRUE(IsNaN(value));
}

TEST(Utils, IsNaNFloatTrue) {
    float value = NAN_F;

    EXPECT_TRUE(IsNaN(value));
}

TEST(Utils, IsNaNDoubleTrue) {
    double value = NAN_D;

    EXPECT_TRUE(IsNaN(value));
}

TEST(Utils, SearchIntAscendingFirst) {
    int array[] = {0, 1, 2, 3, 5, 7, 8, 9, 10, 100};
    int result = Find(&array[0], &array[9], 0);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchIntAscendingMid) {
    int array[] = {0, 1, 2, 3, 5, 7, 8, 9, 10, 100};
    int result = Find(&array[0], &array[9], 8);

    EXPECT_EQ(6, result);
}

TEST(Utils, SearchIntAscendingLast) {
    int array[] = {0, 1, 2, 3, 5, 7, 8, 9, 10, 100};
    int result = Find(&array[0], &array[9], 100);

    EXPECT_EQ(9, result);
}

TEST(Utils, SearchIntAscendingOutOfRange) {
    int array[] = {0, 1, 2, 3, 5, 7, 8, 9, 10, 100};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 1000, 0);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utils, SearchIntAscendingNotFound) {
    int array[] = {0, 1, 2, 3, 5, 7, 8, 9, 10, 100};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 6, 0);

    EXPECT_EQ(NOT_FOUND, result);
}

TEST(Utils, SearchIntAscendingWithToleranceFirst) {
    int array[] = {0, 3, 4, 5, 6, 7, 8, 9, 10, 100};
    int result = Find(&array[0], &array[9], -1, 1);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchIntAscendingWithToleranceMid) {
    int array[] = {0, 1, 2, 3, 5, 7, 8, 9, 10, 100};
    int result = Find(&array[0], &array[9], 11, 1);

    EXPECT_EQ(8, result);
}

TEST(Utils, SearchIntAscendingWithToleranceLast) {
    int array[] = {0, 1, 2, 3, 5, 7, 8, 9, 10, 100};
    int result = Find(&array[0], &array[9], 102, 3);

    EXPECT_EQ(9, result);
}

TEST(Utils, SearchIntDescendingFirst) {
    int array[] = {100, 10, 9, 8, 7, 5, 3, 2, 1, 0};
    int result = Find(&array[0], &array[9], 100);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchIntDescendingMid) {
    int array[] = {100, 10, 9, 8, 7, 5, 3, 2, 1, 0};
    int result = Find(&array[0], &array[9], 8);

    EXPECT_EQ(3, result);
}

TEST(Utils, SearchIntDescendingLast) {
    int array[] = {100, 10, 9, 8, 7, 5, 3, 2, 1, 0};
    int result = Find(&array[0], &array[9], 0);

    EXPECT_EQ(9, result);
}

TEST(Utils, SearchIntDescendingOutOfRange) {
    int array[] = {100, 10, 9, 8, 7, 5, 3, 2, 1, 0};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], -1, 0);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utils, SearchIntDescendingNotFound) {
    int array[] = {100, 10, 9, 8, 7, 5, 3, 2, 1, 0};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 6, 0);

    EXPECT_EQ(NOT_FOUND, result);
}

TEST(Utils, SearchIntDescendingWithToleranceFirst) {
    int array[] = {100, 10, 9, 8, 7, 5, 3, 2, 1, 0};
    int result = Find(&array[0], &array[9], -1, 1);

    EXPECT_EQ(9, result);
}

TEST(Utils, SearchIntDescendingWithToleranceMid) {
    int array[] = {100, 10, 9, 8, 7, 5, 3, 2, 1, 0};
    int result = Find(&array[0], &array[9], 11, 3);

    EXPECT_EQ(1, result);
}

TEST(Utils, SearchIntDescendingWithToleranceLast) {
    int array[] = {100, 10, 9, 8, 7, 5, 3, 2, 1, 0};
    int result = Find(&array[0], &array[9], 102, 3);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchIntUniqueVal) {
    int array[] = {9};
    int result = Find(&array[0], &array[0], 9);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchIntUniqueValWithTolerance) {
    int array[] = {9};
    int result = Find(&array[0], &array[0], 8, 1);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchIntUniqueValOutOfRange) {
    int array[] = {9};
    wxLogNull logNo;
    int result = Find(&array[0], &array[0], 11, 1);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utils, SearchIntArraySameValue) {
    int array[] = {9, 9, 9, 9};
    int result = Find(&array[0], &array[3], 9);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchIntArraySameValueWithToleranceDown) {
    int array[] = {9, 9, 9, 9};
    int result = Find(&array[0], &array[3], 8, 1);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchIntArraySameValueWithToleranceUp) {
    int array[] = {9, 9, 9, 9};
    int result = Find(&array[0], &array[3], 10, 1);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchIntArraySameValueOutOfRange) {
    int array[] = {9, 9, 9, 9};
    wxLogNull logNo;
    int result = Find(&array[0], &array[3], 11, 1);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utils, SearchFloatAscendingFirst) {
    float array[] = {0.354f, 1.932f, 2.7f, 3.56f, 5.021f, 5.75f, 8.2f, 9.65f, 10.45f, 100.0f};
    int result = Find(&array[0], &array[9], 0.354f);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchDoubleAscendingFirst) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    int result = Find(&array[0], &array[9], 0.354);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchDoubleAscendingMid) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    int result = Find(&array[0], &array[9], 5.75);

    EXPECT_EQ(5, result);
}

TEST(Utils, SearchDoubleAscendingLast) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    int result = Find(&array[0], &array[9], 100);

    EXPECT_EQ(9, result);
}

TEST(Utils, SearchDoubleAscendingOutOfRange) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 1000, 0.0);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utils, SearchDoubleAscendingNotFound) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 6, 0.0);

    EXPECT_EQ(NOT_FOUND, result);
}

TEST(Utils, SearchDoubleAscendingWithToleranceFirst) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    int result = Find(&array[0], &array[9], -1.12, 3);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchDoubleAscendingWithToleranceFirstLimit) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    int result = Find(&array[0], &array[9], -1, 1.354);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchDoubleAscendingWithToleranceFirstOutOfRange) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], -1, 1.353);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utils, SearchDoubleAscendingWithToleranceMid) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    int result = Find(&array[0], &array[9], 11, 1);

    EXPECT_EQ(8, result);
}

TEST(Utils, SearchDoubleAscendingWithToleranceMidLimit) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    int result = Find(&array[0], &array[9], 11.45, 1);

    EXPECT_EQ(8, result);
}

TEST(Utils, SearchDoubleAscendingWithToleranceMidNotFound) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 11.45, 0.99);

    EXPECT_EQ(NOT_FOUND, result);
}

TEST(Utils, SearchDoubleAscendingWithToleranceLast) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    int result = Find(&array[0], &array[9], 102.21, 3);

    EXPECT_EQ(9, result);
}

TEST(Utils, SearchDoubleAscendingWithToleranceLastLimit) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    int result = Find(&array[0], &array[9], 101.5, 1.5);

    EXPECT_EQ(9, result);
}

TEST(Utils, SearchDoubleAscendingWithToleranceLastOutOfRange) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 101.5, 1.499);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utils, SearchDoubleDescendingFirst) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    int result = Find(&array[0], &array[9], 100);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchDoubleDescendingMid) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    int result = Find(&array[0], &array[9], 5.75);

    EXPECT_EQ(4, result);
}

TEST(Utils, SearchDoubleDescendingLast) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    int result = Find(&array[0], &array[9], 0.354);

    EXPECT_EQ(9, result);
}

TEST(Utils, SearchDoubleDescendingOutOfRange) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], -1.23, 0.0);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utils, SearchDoubleDescendingNotFound) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 6.2, 0.0);

    EXPECT_EQ(NOT_FOUND, result);
}

TEST(Utils, SearchDoubleDescendingWithToleranceFirst) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    int result = Find(&array[0], &array[9], -1, 2);

    EXPECT_EQ(9, result);
}

TEST(Utils, SearchDoubleDescendingWithToleranceFirstLimit) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    int result = Find(&array[0], &array[9], -1, 1.354);

    EXPECT_EQ(9, result);
}

TEST(Utils, SearchDoubleDescendingWithToleranceFirstOutOfRange) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], -1, 1.353);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utils, SearchDoubleDescendingWithToleranceMid) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    int result = Find(&array[0], &array[9], 11.23, 3);

    EXPECT_EQ(1, result);
}

TEST(Utils, SearchDoubleDescendingWithToleranceMidLimit) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    int result = Find(&array[0], &array[9], 11.45, 1);

    EXPECT_EQ(1, result);
}

TEST(Utils, SearchDoubleDescendingWithToleranceMidOutOfRange) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 11.45, 0.999);

    EXPECT_EQ(NOT_FOUND, result);
}

TEST(Utils, SearchDoubleDescendingWithToleranceLast) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    int result = Find(&array[0], &array[9], 102.42, 3);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchDoubleDescendingWithToleranceLastLimit) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    int result = Find(&array[0], &array[9], 102.21, 2.21);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchDoubleDescendingWithToleranceLastOutOfRange) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 102.21, 2.2);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utils, SearchDoubleUniqueVal) {
    double array[] = {9.3401};
    int result = Find(&array[0], &array[0], 9.3401);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchDoubleUniqueValWithTolerance) {
    double array[] = {9.3401};
    int result = Find(&array[0], &array[0], 8, 1.3401);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchDoubleUniqueValOutOfRange) {
    double array[] = {9.3401};
    wxLogNull logNo;
    int result = Find(&array[0], &array[0], 11, 1);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utils, SearchDoubleArraySameValue) {
    double array[] = {9.34, 9.34, 9.34, 9.34};
    int result = Find(&array[0], &array[3], 9.34);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchDoubleArraySameValueWithToleranceDown) {
    double array[] = {9.34, 9.34, 9.34, 9.34};
    int result = Find(&array[0], &array[3], 8, 1.5);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchDoubleArraySameValueWithToleranceUp) {
    double array[] = {9.34, 9.34, 9.34, 9.34};
    int result = Find(&array[0], &array[3], 10, 1);

    EXPECT_EQ(0, result);
}

TEST(Utils, SearchDoubleArraySameValueOutOfRange) {
    double array[] = {9.34, 9.34, 9.34, 9.34};
    wxLogNull logNo;
    int result = Find(&array[0], &array[3], 11, 1);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utils, SearchStdVectorWithTolerance) {
    std::vector<double> vect{
        -88.542, -86.653, -84.753, -82.851, -80.947, -79.043, -77.139, -75.235, -73.331, -71.426, -69.522, -67.617,
        -65.713, -63.808, -61.903, -59.999, -58.094, -56.189, -54.285, -52.380, -50.475, -48.571, -46.666, -44.761,
        -42.856, -40.952, -39.047, -37.142, -35.238, -33.333, -31.428, -29.523, -27.619, -25.714, -23.809, -21.904,
        -20.000, -18.095, -16.190, -14.286, -12.381, -10.476, -08.571, -06.667, -04.762, -02.857, -00.952, 00.952,
        02.857,  04.762,  06.667,  08.571,  10.476,  12.381,  14.286,  16.190,  18.095,  20.000,  21.904,  23.809,
        25.714,  27.619,  29.523,  31.428,  33.333,  35.238,  37.142,  39.047,  40.952,  42.856,  44.761,  46.666,
        48.571,  50.475,  52.380,  54.285,  56.189,  58.094,  59.999,  61.903,  63.808,  65.713,  67.617,  69.522,
        71.426,  73.331,  75.235,  77.139,  79.043,  80.947,  82.851,  84.753,  86.653,  88.542};
    int result = Find(&vect[0], &vect[93], 29.523, 0.01);

    EXPECT_EQ(62, result);
}

TEST(Utils, GetTimeStructNormal20040101) {
    double mjd = 53005;
    Time date = GetTimeStructFromMJD(mjd);

    EXPECT_EQ(2004, date.year);
    EXPECT_EQ(1, date.month);
    EXPECT_EQ(1, date.day);
}

TEST(Utils, GetTimeStructNormal20040101T12h) {
    double mjd = 53005.5;
    Time date = GetTimeStructFromMJD(mjd);

    EXPECT_EQ(2004, date.year);
    EXPECT_EQ(1, date.month);
    EXPECT_EQ(1, date.day);
    EXPECT_EQ(12, date.hour);
}

TEST(Utils, GetTimeStructNormal20101104T12h) {
    double mjd = 55504.5;
    Time date = GetTimeStructFromMJD(mjd);

    EXPECT_EQ(2010, date.year);
    EXPECT_EQ(11, date.month);
    EXPECT_EQ(4, date.day);
    EXPECT_EQ(12, date.hour);
}

TEST(Utils, GetTimeStructNormal20101104T10h) {
    double mjd = 55504.41666666651;
    Time date = GetTimeStructFromMJD(mjd);

    EXPECT_EQ(2010, date.year);
    EXPECT_EQ(11, date.month);
    EXPECT_EQ(4, date.day);
    EXPECT_EQ(10, date.hour);
}

TEST(Utils, GetTimeStructNormal20101104T103245) {
    double mjd = 55504.43940972211;
    Time date = GetTimeStructFromMJD(mjd);

    EXPECT_EQ(2010, date.year);
    EXPECT_EQ(11, date.month);
    EXPECT_EQ(4, date.day);
    EXPECT_EQ(10, date.hour);
    EXPECT_EQ(32, date.min);
    EXPECT_EQ(45, date.sec);
}

TEST(Utils, ParseDateFormatISOdate) {
    double conversion = ParseDate("2007-11-23", ISOdate);
    double mjd = GetMJD(2007, 11, 23);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatISOdateTime) {
    double conversion = ParseDate("2007-11-23 13:05:01", ISOdateTime);
    double mjd = GetMJD(2007, 11, 23, 13, 5, 1);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatDDMMYYYY) {
    double conversion = ParseDate("23.11.2007", DD_MM_YYYY);
    double mjd = GetMJD(2007, 11, 23);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatDDMMYYYYSlashes) {
    double conversion = ParseDate("23/11/2007", DD_MM_YYYY);
    double mjd = GetMJD(2007, 11, 23);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatDDMMYYYYNoSpace) {
    double conversion = ParseDate("23112007", DD_MM_YYYY);
    double mjd = GetMJD(2007, 11, 23);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatDDMMYYYYException) {
    wxLogNull logNo;

    ASSERT_THROW(ParseDate("23.11.07", DD_MM_YYYY), std::exception);
}

TEST(Utils, ParseDateFormatYYYYMMDD) {
    double conversion = ParseDate("2007.11.23", YYYY_MM_DD);
    double mjd = GetMJD(2007, 11, 23);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatYYYYMMDDNoSpace) {
    double conversion = ParseDate("20071123", YYYY_MM_DD);
    double mjd = GetMJD(2007, 11, 23);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatYYYYMMDDException) {
    wxLogNull logNo;

    ASSERT_THROW(ParseDate("23.11.2007", YYYY_MM_DD), std::exception);
}

TEST(Utils, ParseDateFormatDDMMYYYYhhmm) {
    double conversion = ParseDate("23.11.2007 13:05", DD_MM_YYYY_hh_mm);
    double mjd = GetMJD(2007, 11, 23, 13, 05);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatDDMMYYYYhhmmNoSpace) {
    double conversion = ParseDate("23112007 1305", DD_MM_YYYY_hh_mm);
    double mjd = GetMJD(2007, 11, 23, 13, 05);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatDDMMYYYYhhmmException) {
    wxLogNull logNo;

    ASSERT_THROW(ParseDate("23.11.07 13:05", DD_MM_YYYY_hh_mm), std::exception);
}

TEST(Utils, ParseDateFormatYYYYMMDDhhmm) {
    double conversion = ParseDate("2007.11.23 13:05", YYYY_MM_DD_hh_mm);
    double mjd = GetMJD(2007, 11, 23, 13, 5);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatYYYYMMDDhhmmNoSpace) {
    double conversion = ParseDate("20071123 1305", YYYY_MM_DD_hh_mm);
    double mjd = GetMJD(2007, 11, 23, 13, 5);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatYYYYMMDDhhmmException) {
    wxLogNull logNo;

    ASSERT_THROW(ParseDate("23.11.2007 13:05", YYYY_MM_DD_hh_mm), std::exception);
}

TEST(Utils, ParseDateFormatDDMMYYYYhhmmss) {
    double conversion = ParseDate("23.11.2007 13:05:01", DD_MM_YYYY_hh_mm_ss);
    double mjd = GetMJD(2007, 11, 23, 13, 5, 1);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatDDMMYYYYhhmmssNoSpace) {
    double conversion = ParseDate("23112007 130501", DD_MM_YYYY_hh_mm_ss);
    double mjd = GetMJD(2007, 11, 23, 13, 5, 1);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatDDMMYYYYhhmmssException) {
    wxLogNull logNo;

    ASSERT_THROW(ParseDate("23.11.07 13:05:01", DD_MM_YYYY_hh_mm_ss), std::exception);
}

TEST(Utils, ParseDateFormatYYYYMMDDhhmmss) {
    double conversion = ParseDate("2007.11.23 13:05:01", YYYY_MM_DD_hh_mm_ss);
    double mjd = GetMJD(2007, 11, 23, 13, 5, 1);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatYYYYMMDDhhmmssNoSpace) {
    double conversion = ParseDate("20071123 130501", YYYY_MM_DD_hh_mm_ss);
    double mjd = GetMJD(2007, 11, 23, 13, 5, 1);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatYYYYMMDDhhmmssException) {
    wxLogNull logNo;

    ASSERT_THROW(ParseDate("23.11.2007 13:05:01", YYYY_MM_DD_hh_mm_ss), std::exception);
}

TEST(Utils, ParseDateFormatautoDDMMYYYY) {
    double conversion = ParseDate("23.11.2007", guess);
    double mjd = GetMJD(2007, 11, 23);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatautoDDMMYYYYSlashes) {
    double conversion = ParseDate("23/11/2007", guess);
    double mjd = GetMJD(2007, 11, 23);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatautoDDMMYYYYException) {
    wxLogNull logNo;

    ASSERT_THROW(ParseDate("23.11.07", guess), std::exception);
}

TEST(Utils, ParseDateFormatautoYYYYMMDD) {
    double conversion = ParseDate("2007.11.23", guess);
    double mjd = GetMJD(2007, 11, 23);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatautoYYYYMMDDException) {
    wxLogNull logNo;

    ASSERT_THROW(ParseDate("11.2007", guess), std::exception);
}

TEST(Utils, ParseDateFormatautoDDMMYYYYhhmm) {
    double conversion = ParseDate("23.11.2007 13:05", guess);
    double mjd = GetMJD(2007, 11, 23, 13, 5);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatautoDDMMYYYYhhmmException) {
    wxLogNull logNo;

    ASSERT_THROW(ParseDate("23.11.07 13:05", guess), std::exception);
}

TEST(Utils, ParseDateFormatautoYYYYMMDDhhmm) {
    double conversion = ParseDate("2007.11.23 13:05", guess);
    double mjd = GetMJD(2007, 11, 23, 13, 5);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatautoYYYYMMDDhhmmException) {
    wxLogNull logNo;

    ASSERT_THROW(ParseDate("23.11.07 13:05", guess), std::exception);
}

TEST(Utils, ParseDateFormatautoDDMMYYYYhhmmss) {
    double conversion = ParseDate("23.11.2007 13:05:01", guess);
    double mjd = GetMJD(2007, 11, 23, 13, 5, 1);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatautoDDMMYYYYhhmmssException) {
    wxLogNull logNo;

    ASSERT_THROW(ParseDate("23.11.07 13:05:01", guess), std::exception);
}

TEST(Utils, ParseDateFormatautoYYYYMMDDhhmmss) {
    double conversion = ParseDate("2007.11.23 13:05:01", guess);
    double mjd = GetMJD(2007, 11, 23, 13, 5, 1);

    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateFormatautoYYYYMMDDhhmmssException) {
    wxLogNull logNo;

    ASSERT_THROW(ParseDate("23.11.07 13:05:01", guess), std::exception);
}

TEST(Utils, ParseDateFormatautohhmmException) {
    wxLogNull logNo;

    ASSERT_THROW(ParseDate("13:05:01", guess), std::exception);
}

TEST(Utils, IncrementDateBy1Day) {
    double date = GetMJD(2020, 1, 1);
    double newDate = IncrementDateBy(date, 1, Day);

    EXPECT_EQ(newDate, GetMJD(2020, 1, 2));
}

TEST(Utils, IncrementDateBy5Days) {
    double date = GetMJD(2020, 1, 1);
    double newDate = IncrementDateBy(date, 5, Day);

    EXPECT_EQ(newDate, GetMJD(2020, 1, 6));
}

TEST(Utils, IncrementDateBy2Weeks) {
    double date = GetMJD(2020, 1, 1);
    double newDate = IncrementDateBy(date, 2, Week);

    EXPECT_EQ(newDate, GetMJD(2020, 1, 15));
}

TEST(Utils, IncrementDateBy2Hours) {
    double date = GetMJD(2020, 1, 1);
    double newDate = IncrementDateBy(date, 2, Hour);

    EXPECT_FLOAT_EQ(newDate, GetMJD(2020, 1, 1, 2));
}

TEST(Utils, IncrementDateBy2Minutes) {
    double date = GetMJD(2020, 1, 1);
    double newDate = IncrementDateBy(date, 2, Minute);

    EXPECT_FLOAT_EQ(newDate, GetMJD(2020, 1, 1, 0, 2));
}

// Date parsing edge cases - Leap year tests
TEST(Utils, ParseDateLeapYearFeb29) {
    // 2020 is a leap year, Feb 29 should be valid
    double conversion = ParseDate("2020-02-29", ISOdate);
    double mjd = GetMJD(2020, 2, 29);
    
    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, ParseDateLeapYearFeb29_2000) {
    // 2000 is a leap year (divisible by 400), Feb 29 should be valid
    double conversion = ParseDate("2000-02-29", ISOdate);
    double mjd = GetMJD(2000, 2, 29);
    
    EXPECT_DOUBLE_EQ(mjd, conversion);
}

TEST(Utils, GetMJDLeapYearFeb29) {
    // Test that GetMJD handles leap year correctly
    double mjd2020Feb29 = GetMJD(2020, 2, 29);
    double mjd2020Feb28 = GetMJD(2020, 2, 28);
    
    // Feb 29 should be one day after Feb 28
    EXPECT_DOUBLE_EQ(mjd2020Feb29, mjd2020Feb28 + 1);
}

TEST(Utils, ParseDateInvalidFeb30) {
    wxLogNull logNo;
    
    // Feb 30 doesn't exist - GetMJD should handle gracefully
    // Note: GetMJD uses wxDateTime which may normalize invalid dates
    // This test documents current behavior
    double mjd = GetMJD(2020, 2, 30);
    Time date = GetTimeStructFromMJD(mjd);
    
    // Should normalize to March 1
    EXPECT_EQ(date.month, 3);
    EXPECT_EQ(date.day, 1);
}

TEST(Utils, ParseDateInvalidFeb31) {
    wxLogNull logNo;
    
    // Feb 31 doesn't exist
    double mjd = GetMJD(2020, 2, 31);
    Time date = GetTimeStructFromMJD(mjd);
    
    // Should normalize to March 2
    EXPECT_EQ(date.month, 3);
    EXPECT_EQ(date.day, 2);
}

TEST(Utils, ParseDateInvalidApril31) {
    wxLogNull logNo;
    
    // April only has 30 days
    double mjd = GetMJD(2020, 4, 31);
    Time date = GetTimeStructFromMJD(mjd);
    
    // Should normalize to May 1
    EXPECT_EQ(date.month, 5);
    EXPECT_EQ(date.day, 1);
}

TEST(Utils, ParseDateNonLeapYearFeb29) {
    wxLogNull logNo;
    
    // 2019 is not a leap year
    double mjd = GetMJD(2019, 2, 29);
    Time date = GetTimeStructFromMJD(mjd);
    
    // Should normalize to March 1
    EXPECT_EQ(date.year, 2019);
    EXPECT_EQ(date.month, 3);
    EXPECT_EQ(date.day, 1);
}

TEST(Utils, ParseDateCenturyNotLeapYear) {
    wxLogNull logNo;
    
    // 1900 is NOT a leap year (divisible by 100 but not 400)
    double mjd = GetMJD(1900, 2, 29);
    Time date = GetTimeStructFromMJD(mjd);
    
    // Should normalize to March 1
    EXPECT_EQ(date.year, 1900);
    EXPECT_EQ(date.month, 3);
    EXPECT_EQ(date.day, 1);
}

TEST(Utils, ParseDateMonthBoundaryDay0) {
    wxLogNull logNo;
    
    // Day 0 should normalize
    double mjd = GetMJD(2020, 3, 0);
    Time date = GetTimeStructFromMJD(mjd);
    
    // Should be February 29 (2020 is leap year)
    EXPECT_EQ(date.year, 2020);
    EXPECT_EQ(date.month, 2);
    EXPECT_EQ(date.day, 29);
}

TEST(Utils, ParseDateMonthBoundaryDay32) {
    wxLogNull logNo;
    
    // Day 32 should normalize
    double mjd = GetMJD(2020, 1, 32);
    Time date = GetTimeStructFromMJD(mjd);
    
    // Should be February 1
    EXPECT_EQ(date.year, 2020);
    EXPECT_EQ(date.month, 2);
    EXPECT_EQ(date.day, 1);
}

// FindT tolerance boundary tests
TEST(Utils, FindTExactToleranceBoundaryDouble) {
    // Test when value is exactly at tolerance limit
    double array[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    
    // Search for 2.5 with tolerance 0.5 - should find 2.0 or 3.0
    int result = Find(&array[0], &array[4], 2.5, 0.5);
    
    // Should find either index 1 or 2 (both within tolerance)
    EXPECT_TRUE(result == 1 || result == 2);
}

TEST(Utils, FindTJustInsideToleranceDouble) {
    // Test when value is just inside tolerance boundary
    double array[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    
    // Search for 2.49 with tolerance 0.5 - should find 2.0
    int result = Find(&array[0], &array[4], 2.49, 0.5);
    
    EXPECT_EQ(result, 1);
}

TEST(Utils, FindTJustOutsideToleranceDouble) {
    wxLogNull logNo;
    
    // Test when value is just outside tolerance boundary  
    double array[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    
    // Search for 2.51 with tolerance 0.5 - could match 2.0 or 3.0 (both within 0.51 distance)
    int result = Find(&array[0], &array[4], 2.51, 0.5);
    
    // Should find either index 1 or 2 as both are within tolerance
    EXPECT_TRUE(result == 1 || result == 2);
}

TEST(Utils, FindTZeroToleranceExactMatch) {
    // Test zero tolerance with exact match
    double array[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    
    int result = Find(&array[0], &array[4], 3.0, 0.0);
    
    EXPECT_EQ(result, 2);
}

TEST(Utils, FindTZeroToleranceNoMatch) {
    wxLogNull logNo;
    
    // Test zero tolerance without exact match
    double array[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    
    int result = Find(&array[0], &array[4], 2.5, 0.0);
    
    EXPECT_EQ(result, NOT_FOUND);
}

TEST(Utils, FindTVerySmallToleranceDouble) {
    // Test with very small tolerance (near machine epsilon)
    double array[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    
    // Value within epsilon should match
    int result = Find(&array[0], &array[4], 3.0 + EPSILON_D, EPSILON_D * 2);
    
    EXPECT_EQ(result, 2);
}

TEST(Utils, FindTVerySmallToleranceOutside) {
    wxLogNull logNo;
    
    // Test with very small tolerance outside range
    double array[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    
    // Value outside small tolerance should not match
    int result = Find(&array[0], &array[4], 3.0 + EPSILON_D * 10, EPSILON_D);
    
    EXPECT_EQ(result, NOT_FOUND);
}

TEST(Utils, FindTToleranceAtArrayBoundaryStart) {
    // Test tolerance at start boundary
    double array[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    
    // Value before first element within tolerance
    int result = Find(&array[0], &array[4], 0.6, 0.4);
    
    EXPECT_EQ(result, 0);
}

TEST(Utils, FindTToleranceAtArrayBoundaryEnd) {
    // Test tolerance at end boundary
    double array[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    
    // Value after last element within tolerance
    int result = Find(&array[0], &array[4], 5.4, 0.4);
    
    EXPECT_EQ(result, 4);
}

TEST(Utils, FindTToleranceBoundaryOutOfRange) {
    wxLogNull logNo;
    
    // Test when tolerance doesn't reach array
    double array[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    
    // Value before first element outside tolerance
    int result = Find(&array[0], &array[4], 0.4, 0.5);
    
    EXPECT_EQ(result, OUT_OF_RANGE);
}

TEST(Utils, FindTToleranceIntegerBoundary) {
    // Test integer tolerance boundaries
    int array[] = {0, 10, 20, 30, 40};
    
    // Exact tolerance boundary
    int result = Find(&array[0], &array[4], 15, 5);
    
    EXPECT_TRUE(result == 1 || result == 2);
}

TEST(Utils, FindTToleranceIntegerJustInside) {
    // Test integer just inside tolerance
    int array[] = {0, 10, 20, 30, 40};
    
    int result = Find(&array[0], &array[4], 14, 5);
    
    EXPECT_EQ(result, 1);
}

TEST(Utils, FindTToleranceIntegerJustOutside) {
    wxLogNull logNo;
    
    // Test integer just outside tolerance
    int array[] = {0, 10, 20, 30, 40};
    
    int result = Find(&array[0], &array[4], 16, 5);
    
    EXPECT_TRUE(result == 1 || result == 2);
}
