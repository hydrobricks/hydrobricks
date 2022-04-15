#include <gtest/gtest.h>

#include "ModelHydro.h"

TEST(TimeSeries, VariableType) {
    TimeSeries series(Precipitation);

    EXPECT_EQ(Precipitation, series.GetVariableType());
}

TEST(TimeSeriesDataRegular, SetValueOK) {
    TimeSeriesDataRegular tsData = TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                                         wxDateTime(10, wxDateTime::Jan, 2020),
                                                         1, Day);
    std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};

    EXPECT_TRUE(tsData.SetValues(values));
}

TEST(TimeSeriesDataRegular, SetValueTooLong) {
    TimeSeriesDataRegular tsData = TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                                         wxDateTime(10, wxDateTime::Jan, 2020),
                                                         1, Day);
    std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0};

    EXPECT_FALSE(tsData.SetValues(values));
}

TEST(TimeSeriesDataRegular, SetValueTooShort) {
    TimeSeriesDataRegular tsData = TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                                         wxDateTime(10, wxDateTime::Jan, 2020),
                                                         1, Day);
    std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0};

    EXPECT_FALSE(tsData.SetValues(values));
}