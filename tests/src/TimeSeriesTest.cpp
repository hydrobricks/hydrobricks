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

TEST(TimeSeries, ParseFile) {
    std::vector<TimeSeries*> vecTimeSeries;
    EXPECT_TRUE(TimeSeries::Parse("files/time-series-data.nc", vecTimeSeries));

    EXPECT_EQ(vecTimeSeries.size(), 3);
    EXPECT_EQ(vecTimeSeries[0]->GetVariableType(), Temperature);
    EXPECT_EQ(vecTimeSeries[1]->GetVariableType(), Precipitation);
    EXPECT_EQ(vecTimeSeries[2]->GetVariableType(), PET);

    wxDateTime date = wxDateTime(20, wxDateTime::Oct, 2014);
    EXPECT_FLOAT_EQ(vecTimeSeries[0]->GetDataPointer(3)->GetValueFor(date), 25.31445313);
    EXPECT_FLOAT_EQ(vecTimeSeries[0]->GetDataPointer(11)->GetValueFor(date), 23.58593750);

    date = wxDateTime(27, wxDateTime::Nov, 2014);
    EXPECT_FLOAT_EQ(vecTimeSeries[1]->GetDataPointer(5)->GetValueFor(date), 8.23046875);
}