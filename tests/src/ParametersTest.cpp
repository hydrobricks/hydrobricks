
#include <gtest/gtest.h>

#include "ParameterVariable.h"
#include "ParametersUpdater.h"

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