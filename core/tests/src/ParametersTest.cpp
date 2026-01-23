#include <gtest/gtest.h>

#include "Parameter.h"
#include "ParameterModifier.h"
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

TEST(YearlyModifier, SetValues) {
    ParameterModifier modifier(ParameterModifierType::Yearly);
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

    EXPECT_TRUE(modifier.SetYearlyValues(2000, 2010, values));
}

TEST(YearlyModifier, SetValuesWrongSize) {
    ParameterModifier modifier(ParameterModifierType::Yearly);
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    wxLogNull logNo;

    EXPECT_FALSE(modifier.SetYearlyValues(2000, 2011, values));
}

TEST(YearlyModifier, UpdateValue) {
    Parameter parameter("dummy_parameter");
    ParameterModifier modifier(ParameterModifierType::Yearly);
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    modifier.SetYearlyValues(2000, 2010, values);
    parameter.SetModifier(modifier);
    parameter.UpdateFromModifier(GetMJD(2005, 6, 15));

    EXPECT_EQ(parameter.GetValue(), 6);
}

TEST(YearlyModifier, UpdateValueNotFound) {
    wxLogNull logNo;
    Parameter parameter("dummy_parameter");
    ParameterModifier modifier(ParameterModifierType::Yearly);
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    modifier.SetYearlyValues(2000, 2010, values);
    parameter.SetModifier(modifier);
    EXPECT_FALSE(parameter.UpdateFromModifier(GetMJD(2015, 1, 1)));
    EXPECT_TRUE(IsNaN(parameter.GetValue()));
}

TEST(MonthlyModifier, SetValues) {
    ParameterModifier modifier(ParameterModifierType::Monthly);
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

    EXPECT_TRUE(modifier.SetMonthlyValues(values));
}

TEST(MonthlyModifier, SetValuesWrongSize) {
    ParameterModifier modifier(ParameterModifierType::Monthly);
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    wxLogNull logNo;

    EXPECT_FALSE(modifier.SetMonthlyValues(values));
}

TEST(MonthlyModifier, UpdateValue) {
    Parameter parameter("dummy_parameter");
    ParameterModifier modifier(ParameterModifierType::Monthly);
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    modifier.SetMonthlyValues(values);
    parameter.SetModifier(modifier);
    parameter.UpdateFromModifier(GetMJD(2010, 3, 15));

    EXPECT_EQ(parameter.GetValue(), 3);
}

TEST(DatesModifier, SetValues) {
    ParameterModifier modifier(ParameterModifierType::Dates);
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<double> dates = GenerateDailyDatesVector();

    EXPECT_TRUE(modifier.SetDatesAndValues(dates, values));
}

TEST(DatesModifier, SetValuesWrongSize) {
    ParameterModifier modifier(ParameterModifierType::Dates);
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    std::vector<double> dates = GenerateDailyDatesVector();
    wxLogNull logNo;

    EXPECT_FALSE(modifier.SetDatesAndValues(dates, values));
}

TEST(DatesModifier, UpdateValue) {
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<double> dates = GenerateDailyDatesVector();
    Parameter parameter("dummy_parameter");
    ParameterModifier modifier(ParameterModifierType::Dates);
    modifier.SetDatesAndValues(dates, values);
    parameter.SetModifier(modifier);
    double dateExtract = GetMJD(2010, 1, 4);
    parameter.UpdateFromModifier(dateExtract);

    EXPECT_EQ(parameter.GetValue(), 4);
}

TEST(DatesModifier, UpdateValueNotFound) {
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<double> dates = GenerateDailyDatesVector();
    wxLogNull logNo;
    Parameter parameter("dummy_parameter");
    ParameterModifier modifier(ParameterModifierType::Dates);
    modifier.SetDatesAndValues(dates, values);
    parameter.SetModifier(modifier);
    double dateExtract = GetMJD(2010, 1, 20);

    EXPECT_FALSE(parameter.UpdateFromModifier(dateExtract));
    EXPECT_TRUE(IsNaN(parameter.GetValue()));
}

TEST(ParametersUpdater, DateUpdateDay) {
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<double> dates = GenerateDailyDatesVector();
    Parameter parameter("dummy_parameter");
    ParameterModifier modifier(ParameterModifierType::Dates);
    modifier.SetDatesAndValues(dates, values);
    parameter.SetModifier(modifier);

    ParametersUpdater updater;
    updater.AddParameter(&parameter);
    updater.DateUpdate(GetMJD(2010, 2, 1));
    updater.DateUpdate(GetMJD(2010, 1, 4));

    EXPECT_EQ(parameter.GetValue(), 4);
}

TEST(ParametersUpdater, DateUpdateMonth) {
    Parameter parameter("dummy_parameter");
    ParameterModifier modifier(ParameterModifierType::Monthly);
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    modifier.SetMonthlyValues(values);
    parameter.SetModifier(modifier);

    ParametersUpdater updater;
    updater.AddParameter(&parameter);
    updater.DateUpdate(GetMJD(2010, 2, 1));
    updater.DateUpdate(GetMJD(2010, 3, 1));

    EXPECT_EQ(parameter.GetValue(), 3);
}

TEST(ParametersUpdater, DateUpdateYear) {
    Parameter parameter("dummy_parameter");
    ParameterModifier modifier(ParameterModifierType::Yearly);
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8};
    modifier.SetYearlyValues(2011, 2018, values);
    parameter.SetModifier(modifier);

    ParametersUpdater updater;
    updater.AddParameter(&parameter);
    updater.DateUpdate(GetMJD(2014, 1, 1));
    updater.DateUpdate(GetMJD(2015, 3, 13));

    EXPECT_EQ(parameter.GetValue(), 5);
}
