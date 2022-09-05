#include <gtest/gtest.h>

#include "ParameterVariable.h"
#include "ParametersUpdater.h"

std::vector<double> GenerateDailyDatesVector() {
    std::vector<double> dates;
    double date = GetMJD(2010, 1, 1);
    for (int i = 0; i < 10; ++i) {
        dates.push_back(date);
        date += 1;
    }

    return dates;
}

TEST(ParameterVariableYearly, SetValues) {
    ParameterVariableYearly parameter("dummyParameter");
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

    EXPECT_TRUE(parameter.SetValues(2000, 2010, values));
}

TEST(ParameterVariableYearly, SetValuesWrongSize) {
    ParameterVariableYearly parameter("dummyParameter");
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    wxLogNull logNo;

    EXPECT_FALSE(parameter.SetValues(2000, 2011, values));
}

TEST(ParameterVariableYearly, UpdateParameter) {
    ParameterVariableYearly parameter("dummyParameter");
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    parameter.SetValues(2000, 2010, values);

    parameter.UpdateParameter(2005);

    EXPECT_EQ(parameter.GetValue(), 6);
}

TEST(ParameterVariableYearly, UpdateParameterNotFound) {
    ParameterVariableYearly parameter("dummyParameter");
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    parameter.SetValues(2000, 2010, values);
    wxLogNull logNo;

    EXPECT_FALSE(parameter.UpdateParameter(2015));
    EXPECT_TRUE(IsNaN(parameter.GetValue()));
}

TEST(ParameterVariableMonthly, SetValues) {
    ParameterVariableMonthly parameter("dummyParameter");
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

    EXPECT_TRUE(parameter.SetValues(values));
}

TEST(ParameterVariableMonthly, SetValuesWrongSize) {
    ParameterVariableMonthly parameter("dummyParameter");
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    wxLogNull logNo;

    EXPECT_FALSE(parameter.SetValues(values));
}

TEST(ParameterVariableMonthly, UpdateParameter) {
    ParameterVariableMonthly parameter("dummyParameter");
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    parameter.SetValues(values);

    parameter.UpdateParameter(3);

    EXPECT_EQ(parameter.GetValue(), 3);
}

TEST(ParameterVariableDates, SetValues) {
    ParameterVariableDates parameter("dummyParameter");
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<double> dates = GenerateDailyDatesVector();

    EXPECT_TRUE(parameter.SetTimeAndValues(dates, values));
}

TEST(ParameterVariableDates, SetValuesWrongSize) {
    ParameterVariableDates parameter("dummyParameter");
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    std::vector<double> dates = GenerateDailyDatesVector();
    wxLogNull logNo;

    EXPECT_FALSE(parameter.SetTimeAndValues(dates, values));
}

TEST(ParameterVariableDates, UpdateParameter) {
    ParameterVariableDates parameter("dummyParameter");
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<double> dates = GenerateDailyDatesVector();
    parameter.SetTimeAndValues(dates, values);

    double dateExtract = GetMJD(2010, 1, 4);
    parameter.UpdateParameter(dateExtract);

    EXPECT_EQ(parameter.GetValue(), 4);
}

TEST(ParameterVariableDates, UpdateParameterNotFound) {
    ParameterVariableDates parameter("dummyParameter");
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<double> dates = GenerateDailyDatesVector();
    parameter.SetTimeAndValues(dates, values);

    wxLogNull logNo;
    double dateExtract = GetMJD(2010, 1, 20);

    EXPECT_FALSE(parameter.UpdateParameter(dateExtract));
    EXPECT_TRUE(IsNaN(parameter.GetValue()));
}

TEST(ParametersUpdater, DateUpdateDay) {
    ParameterVariableDates parameter("dummyParameter");
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<double> dates = GenerateDailyDatesVector();
    parameter.SetTimeAndValues(dates, values);

    ParametersUpdater updater;
    updater.AddParameterVariableDates(&parameter);
    updater.DateUpdate(GetMJD(2010, 2, 1));
    updater.DateUpdate(GetMJD(2010, 1, 4));

    EXPECT_EQ(parameter.GetValue(), 4);
}

TEST(ParametersUpdater, DateUpdateMonth) {
    ParameterVariableMonthly parameter("dummyParameter");
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    parameter.SetValues(values);

    ParametersUpdater updater;
    updater.AddParameterVariableMonthly(&parameter);
    updater.DateUpdate(GetMJD(2010, 2, 1));
    updater.DateUpdate(GetMJD(2010, 3, 1));

    EXPECT_EQ(parameter.GetValue(), 3);
}

TEST(ParametersUpdater, DateUpdateYear) {
    ParameterVariableYearly parameter("dummyParameter");
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8};
    parameter.SetValues(2011, 2018, values);

    ParametersUpdater updater;
    updater.AddParameterVariableYearly(&parameter);
    updater.DateUpdate(GetMJD(2014, 1, 1));
    updater.DateUpdate(GetMJD(2015, 3, 13));

    EXPECT_EQ(parameter.GetValue(), 5);
}