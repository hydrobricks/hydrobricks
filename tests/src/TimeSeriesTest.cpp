#include <gtest/gtest.h>

#include "TimeSeriesUniform.h"
#include "TimeSeriesData.h"

TEST(TimeSeries, VariableType) {
    TimeSeriesUniform series(Precipitation);

    EXPECT_EQ(Precipitation, series.GetVariableType());
}

TEST(TimeSeriesDataRegular, SetValueOK) {
    TimeSeriesDataRegular tsData = TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                                         wxDateTime(10, wxDateTime::Jan, 2020), 1, Day);
    EXPECT_TRUE(tsData.SetValues({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0}));
}

TEST(TimeSeriesDataRegular, SetValueTooLong) {
    wxLogNull logNo;

    TimeSeriesDataRegular tsData = TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                                         wxDateTime(10, wxDateTime::Jan, 2020), 1, Day);
    EXPECT_FALSE(tsData.SetValues({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0}));
}

TEST(TimeSeriesDataRegular, SetValueTooShort) {
    wxLogNull logNo;

    TimeSeriesDataRegular tsData = TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                                         wxDateTime(10, wxDateTime::Jan, 2020), 1, Day);
    EXPECT_FALSE(tsData.SetValues({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0}));
}