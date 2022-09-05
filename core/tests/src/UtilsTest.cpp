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
