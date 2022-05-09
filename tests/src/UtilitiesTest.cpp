#include <gtest/gtest.h>

#include "Utilities.h"

TEST(Utilities, ZeroIsNotNaN) {
    int value = 0;

    EXPECT_FALSE(IsNaN((float)value));
}

TEST(Utilities, IsNaNIntTrue) {
    int value = NaNi;

    EXPECT_TRUE(IsNaN(value));
}

TEST(Utilities, IsNaNFloatTrue) {
    float value = NaNf;

    EXPECT_TRUE(IsNaN(value));
}

TEST(Utilities, IsNaNDoubleTrue) {
    double value = NaNd;

    EXPECT_TRUE(IsNaN(value));
}

TEST(Utilities, SearchIntAscendingFirst) {
    int array[] = {0, 1, 2, 3, 5, 7, 8, 9, 10, 100};
    int result = Find(&array[0], &array[9], 0);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchIntAscendingMid) {
    int array[] = {0, 1, 2, 3, 5, 7, 8, 9, 10, 100};
    int result = Find(&array[0], &array[9], 8);

    EXPECT_EQ(6, result);
}

TEST(Utilities, SearchIntAscendingLast) {
    int array[] = {0, 1, 2, 3, 5, 7, 8, 9, 10, 100};
    int result = Find(&array[0], &array[9], 100);

    EXPECT_EQ(9, result);
}

TEST(Utilities, SearchIntAscendingOutOfRange) {
    int array[] = {0, 1, 2, 3, 5, 7, 8, 9, 10, 100};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 1000, 0);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utilities, SearchIntAscendingNotFound) {
    int array[] = {0, 1, 2, 3, 5, 7, 8, 9, 10, 100};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 6, 0);

    EXPECT_EQ(NOT_FOUND, result);
}

TEST(Utilities, SearchIntAscendingWithToleranceFirst) {
    int array[] = {0, 3, 4, 5, 6, 7, 8, 9, 10, 100};
    int result = Find(&array[0], &array[9], -1, 1);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchIntAscendingWithToleranceMid) {
    int array[] = {0, 1, 2, 3, 5, 7, 8, 9, 10, 100};
    int result = Find(&array[0], &array[9], 11, 1);

    EXPECT_EQ(8, result);
}

TEST(Utilities, SearchIntAscendingWithToleranceLast) {
    int array[] = {0, 1, 2, 3, 5, 7, 8, 9, 10, 100};
    int result = Find(&array[0], &array[9], 102, 3);

    EXPECT_EQ(9, result);
}

TEST(Utilities, SearchIntDescendingFirst) {
    int array[] = {100, 10, 9, 8, 7, 5, 3, 2, 1, 0};
    int result = Find(&array[0], &array[9], 100);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchIntDescendingMid) {
    int array[] = {100, 10, 9, 8, 7, 5, 3, 2, 1, 0};
    int result = Find(&array[0], &array[9], 8);

    EXPECT_EQ(3, result);
}

TEST(Utilities, SearchIntDescendingLast) {
    int array[] = {100, 10, 9, 8, 7, 5, 3, 2, 1, 0};
    int result = Find(&array[0], &array[9], 0);

    EXPECT_EQ(9, result);
}

TEST(Utilities, SearchIntDescendingOutOfRange) {
    int array[] = {100, 10, 9, 8, 7, 5, 3, 2, 1, 0};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], -1, 0);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utilities, SearchIntDescendingNotFound) {
    int array[] = {100, 10, 9, 8, 7, 5, 3, 2, 1, 0};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 6, 0);

    EXPECT_EQ(NOT_FOUND, result);
}

TEST(Utilities, SearchIntDescendingWithToleranceFirst) {
    int array[] = {100, 10, 9, 8, 7, 5, 3, 2, 1, 0};
    int result = Find(&array[0], &array[9], -1, 1);

    EXPECT_EQ(9, result);
}

TEST(Utilities, SearchIntDescendingWithToleranceMid) {
    int array[] = {100, 10, 9, 8, 7, 5, 3, 2, 1, 0};
    int result = Find(&array[0], &array[9], 11, 3);

    EXPECT_EQ(1, result);
}

TEST(Utilities, SearchIntDescendingWithToleranceLast) {
    int array[] = {100, 10, 9, 8, 7, 5, 3, 2, 1, 0};
    int result = Find(&array[0], &array[9], 102, 3);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchIntUniqueVal) {
    int array[] = {9};
    int result = Find(&array[0], &array[0], 9);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchIntUniqueValWithTolerance) {
    int array[] = {9};
    int result = Find(&array[0], &array[0], 8, 1);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchIntUniqueValOutOfRange) {
    int array[] = {9};
    wxLogNull logNo;
    int result = Find(&array[0], &array[0], 11, 1);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utilities, SearchIntArraySameValue) {
    int array[] = {9, 9, 9, 9};
    int result = Find(&array[0], &array[3], 9);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchIntArraySameValueWithToleranceDown) {
    int array[] = {9, 9, 9, 9};
    int result = Find(&array[0], &array[3], 8, 1);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchIntArraySameValueWithToleranceUp) {
    int array[] = {9, 9, 9, 9};
    int result = Find(&array[0], &array[3], 10, 1);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchIntArraySameValueOutOfRange) {
    int array[] = {9, 9, 9, 9};
    wxLogNull logNo;
    int result = Find(&array[0], &array[3], 11, 1);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utilities, SearchFloatAscendingFirst) {
    float array[] = {0.354f, 1.932f, 2.7f, 3.56f, 5.021f, 5.75f, 8.2f, 9.65f, 10.45f, 100.0f};
    int result = Find(&array[0], &array[9], 0.354f);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchDoubleAscendingFirst) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    int result = Find(&array[0], &array[9], 0.354);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchDoubleAscendingMid) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    int result = Find(&array[0], &array[9], 5.75);

    EXPECT_EQ(5, result);
}

TEST(Utilities, SearchDoubleAscendingLast) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    int result = Find(&array[0], &array[9], 100);

    EXPECT_EQ(9, result);
}

TEST(Utilities, SearchDoubleAscendingOutOfRange) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 1000, 0.0);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utilities, SearchDoubleAscendingNotFound) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 6, 0.0);

    EXPECT_EQ(NOT_FOUND, result);
}

TEST(Utilities, SearchDoubleAscendingWithToleranceFirst) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    int result = Find(&array[0], &array[9], -1.12, 3);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchDoubleAscendingWithToleranceFirstLimit) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    int result = Find(&array[0], &array[9], -1, 1.354);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchDoubleAscendingWithToleranceFirstOutOfRange) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], -1, 1.353);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utilities, SearchDoubleAscendingWithToleranceMid) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    int result = Find(&array[0], &array[9], 11, 1);

    EXPECT_EQ(8, result);
}

TEST(Utilities, SearchDoubleAscendingWithToleranceMidLimit) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    int result = Find(&array[0], &array[9], 11.45, 1);

    EXPECT_EQ(8, result);
}

TEST(Utilities, SearchDoubleAscendingWithToleranceMidNotFound) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 11.45, 0.99);

    EXPECT_EQ(NOT_FOUND, result);
}

TEST(Utilities, SearchDoubleAscendingWithToleranceLast) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    int result = Find(&array[0], &array[9], 102.21, 3);

    EXPECT_EQ(9, result);
}

TEST(Utilities, SearchDoubleAscendingWithToleranceLastLimit) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    int result = Find(&array[0], &array[9], 101.5, 1.5);

    EXPECT_EQ(9, result);
}

TEST(Utilities, SearchDoubleAscendingWithToleranceLastOutOfRange) {
    double array[] = {0.354, 1.932, 2.7, 3.56, 5.021, 5.75, 8.2, 9.65, 10.45, 100};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 101.5, 1.499);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utilities, SearchDoubleDescendingFirst) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    int result = Find(&array[0], &array[9], 100);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchDoubleDescendingMid) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    int result = Find(&array[0], &array[9], 5.75);

    EXPECT_EQ(4, result);
}

TEST(Utilities, SearchDoubleDescendingLast) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    int result = Find(&array[0], &array[9], 0.354);

    EXPECT_EQ(9, result);
}

TEST(Utilities, SearchDoubleDescendingOutOfRange) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], -1.23, 0.0);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utilities, SearchDoubleDescendingNotFound) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 6.2, 0.0);

    EXPECT_EQ(NOT_FOUND, result);
}

TEST(Utilities, SearchDoubleDescendingWithToleranceFirst) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    int result = Find(&array[0], &array[9], -1, 2);

    EXPECT_EQ(9, result);
}

TEST(Utilities, SearchDoubleDescendingWithToleranceFirstLimit) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    int result = Find(&array[0], &array[9], -1, 1.354);

    EXPECT_EQ(9, result);
}

TEST(Utilities, SearchDoubleDescendingWithToleranceFirstOutOfRange) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], -1, 1.353);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utilities, SearchDoubleDescendingWithToleranceMid) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    int result = Find(&array[0], &array[9], 11.23, 3);

    EXPECT_EQ(1, result);
}

TEST(Utilities, SearchDoubleDescendingWithToleranceMidLimit) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    int result = Find(&array[0], &array[9], 11.45, 1);

    EXPECT_EQ(1, result);
}

TEST(Utilities, SearchDoubleDescendingWithToleranceMidOutOfRange) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 11.45, 0.999);

    EXPECT_EQ(NOT_FOUND, result);
}

TEST(Utilities, SearchDoubleDescendingWithToleranceLast) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    int result = Find(&array[0], &array[9], 102.42, 3);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchDoubleDescendingWithToleranceLastLimit) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    int result = Find(&array[0], &array[9], 102.21, 2.21);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchDoubleDescendingWithToleranceLastOutOfRange) {
    double array[] = {100, 10.45, 9.65, 8.2, 5.75, 5.021, 3.56, 2.7, 1.932, 0.354};
    wxLogNull logNo;
    int result = Find(&array[0], &array[9], 102.21, 2.2);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utilities, SearchDoubleUniqueVal) {
    double array[] = {9.3401};
    int result = Find(&array[0], &array[0], 9.3401);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchDoubleUniqueValWithTolerance) {
    double array[] = {9.3401};
    int result = Find(&array[0], &array[0], 8, 1.3401);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchDoubleUniqueValOutOfRange) {
    double array[] = {9.3401};
    wxLogNull logNo;
    int result = Find(&array[0], &array[0], 11, 1);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utilities, SearchDoubleArraySameValue) {
    double array[] = {9.34, 9.34, 9.34, 9.34};
    int result = Find(&array[0], &array[3], 9.34);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchDoubleArraySameValueWithToleranceDown) {
    double array[] = {9.34, 9.34, 9.34, 9.34};
    int result = Find(&array[0], &array[3], 8, 1.5);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchDoubleArraySameValueWithToleranceUp) {
    double array[] = {9.34, 9.34, 9.34, 9.34};
    int result = Find(&array[0], &array[3], 10, 1);

    EXPECT_EQ(0, result);
}

TEST(Utilities, SearchDoubleArraySameValueOutOfRange) {
    double array[] = {9.34, 9.34, 9.34, 9.34};
    wxLogNull logNo;
    int result = Find(&array[0], &array[3], 11, 1);

    EXPECT_EQ(OUT_OF_RANGE, result);
}

TEST(Utilities, SearchStdVectorWithTolerance) {
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

TEST(Utilities, IncrementDateBy1Day) {
    wxDateTime date = wxDateTime(1, wxDateTime::Jan, 2020);
    wxDateTime newDate = IncrementDateBy(date, 1, Day);

    EXPECT_TRUE(newDate.IsSameDate(wxDateTime(2, wxDateTime::Jan, 2020)));
}

TEST(Utilities, IncrementDateBy5Days) {
    wxDateTime date = wxDateTime(1, wxDateTime::Jan, 2020);
    wxDateTime newDate = IncrementDateBy(date, 5, Day);

    EXPECT_TRUE(newDate.IsSameDate(wxDateTime(6, wxDateTime::Jan, 2020)));
}

TEST(Utilities, IncrementDateBy2Weeks) {
    wxDateTime date = wxDateTime(1, wxDateTime::Jan, 2020);
    wxDateTime newDate = IncrementDateBy(date, 2, Week);

    EXPECT_TRUE(newDate.IsSameDate(wxDateTime(15, wxDateTime::Jan, 2020)));
}

TEST(Utilities, IncrementDateBy2Hours) {
    wxDateTime date = wxDateTime(1, wxDateTime::Jan, 2020);
    wxDateTime newDate = IncrementDateBy(date, 2, Hour);

    EXPECT_TRUE(newDate.IsSameDate(wxDateTime(1, wxDateTime::Jan, 2020, 2)));
}

TEST(Utilities, IncrementDateBy2Minutes) {
    wxDateTime date = wxDateTime(1, wxDateTime::Jan, 2020);
    wxDateTime newDate = IncrementDateBy(date, 2, Minute);

    EXPECT_TRUE(newDate.IsSameDate(wxDateTime(1, wxDateTime::Jan, 2020, 0, 2)));
}