#include <gtest/gtest.h>

#include "ParameterVariable.h"
#include "ParametersUpdater.h"

std::vector<double> GenerateDailyDatesVector() {
    std::vector<double> dates;
    wxDateTime date(1, wxDateTime::Jan, 2010);
    for (int i = 0; i < 10; ++i) {
        dates.push_back(date.GetJDN());
        date = date.Add(wxDateSpan(0, 0, 0, 1));
    }

    return dates;
}

TEST(ParameterVariableYearly, SetValues) {
    ParameterVariableYearly parameter;
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

    EXPECT_TRUE(parameter.SetValues(2000, 2010, values));
}

TEST(ParameterVariableYearly, SetValuesWrongSize) {
    ParameterVariableYearly parameter;
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    wxLogNull logNo;

    EXPECT_FALSE(parameter.SetValues(2000, 2011, values));
}

TEST(ParameterVariableYearly, UpdateParameter) {
    ParameterVariableYearly parameter;
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    parameter.SetValues(2000, 2010, values);

    parameter.UpdateParameter(2005);

    EXPECT_EQ(6, parameter.GetValue());
}

TEST(ParameterVariableYearly, UpdateParameterNotFound) {
    ParameterVariableYearly parameter;
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    parameter.SetValues(2000, 2010, values);
    wxLogNull logNo;

    EXPECT_FALSE(parameter.UpdateParameter(2015));
    EXPECT_TRUE(IsNaN(parameter.GetValue()));
}

TEST(ParameterVariableMonthly, SetValues) {
    ParameterVariableMonthly parameter;
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

    EXPECT_TRUE(parameter.SetValues(values));
}

TEST(ParameterVariableMonthly, SetValuesWrongSize) {
    ParameterVariableMonthly parameter;
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    wxLogNull logNo;

    EXPECT_FALSE(parameter.SetValues(values));
}

TEST(ParameterVariableMonthly, UpdateParameter) {
    ParameterVariableMonthly parameter;
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    parameter.SetValues(values);

    parameter.UpdateParameter(wxDateTime::Mar);

    EXPECT_EQ(3, parameter.GetValue());
}

TEST(ParameterVariableDates, SetValues) {
    ParameterVariableDates parameter;
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<double> dates = GenerateDailyDatesVector();

    EXPECT_TRUE(parameter.SetTimeAndValues(dates, values));
}

TEST(ParameterVariableDates, SetValuesWrongSize) {
    ParameterVariableDates parameter;
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    std::vector<double> dates = GenerateDailyDatesVector();
    wxLogNull logNo;

    EXPECT_FALSE(parameter.SetTimeAndValues(dates, values));
}

TEST(ParameterVariableDates, UpdateParameter) {
    ParameterVariableDates parameter;
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<double> dates = GenerateDailyDatesVector();
    parameter.SetTimeAndValues(dates, values);

    wxDateTime dateExtract(4, wxDateTime::Jan, 2010);
    parameter.UpdateParameter(dateExtract.GetJDN());

    EXPECT_EQ(4, parameter.GetValue());
}

TEST(ParameterVariableDates, UpdateParameterNotFound) {
    ParameterVariableDates parameter;
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<double> dates = GenerateDailyDatesVector();
    parameter.SetTimeAndValues(dates, values);

    wxLogNull logNo;
    wxDateTime dateExtract(20, wxDateTime::Jan, 2010);

    EXPECT_FALSE(parameter.UpdateParameter(dateExtract.GetJDN()));
    EXPECT_TRUE(IsNaN(parameter.GetValue()));
}

TEST(ParametersUpdater, DateUpdateDay) {
    ParameterVariableDates parameter;
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<double> dates = GenerateDailyDatesVector();
    parameter.SetTimeAndValues(dates, values);

    ParametersUpdater updater;
    updater.AddParameterVariableDates(&parameter);
    updater.DateUpdate(wxDateTime(1, wxDateTime::Jan, 2010));
    updater.DateUpdate(wxDateTime(4, wxDateTime::Jan, 2010));

    EXPECT_EQ(4, parameter.GetValue());
}

TEST(ParametersUpdater, DateUpdateMonth) {
    ParameterVariableMonthly parameter;
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    parameter.SetValues(values);

    ParametersUpdater updater;
    updater.AddParameterVariableMonthly(&parameter);
    updater.DateUpdate(wxDateTime(1, wxDateTime::Feb, 2010));
    updater.DateUpdate(wxDateTime(1, wxDateTime::Mar, 2010));

    EXPECT_EQ(3, parameter.GetValue());
}

TEST(ParametersUpdater, DateUpdateYear) {
    ParameterVariableYearly parameter;
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8};
    parameter.SetValues(2011, 2018, values);

    ParametersUpdater updater;
    updater.AddParameterVariableYearly(&parameter);
    updater.DateUpdate(wxDateTime(1, wxDateTime::Jan, 2014));
    updater.DateUpdate(wxDateTime(13, wxDateTime::Mar, 2015));

    EXPECT_EQ(5, parameter.GetValue());
}