#include <gtest/gtest.h>

#include "HydroUnit.h"
#include "HydroUnitProperty.h"
#include "Parameter.h"
#include "ParameterModifier.h"
#include "ParametersUpdater.h"
#include "SettingsModel.h"

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

TEST(ParametersUpdater, ResetClearsRegistration) {
    Parameter parameter("dummy_parameter");
    ParameterModifier modifier(ParameterModifierType::Monthly);
    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    modifier.SetMonthlyValues(values);
    parameter.SetModifier(modifier);
    parameter.SetValue(-1);

    ParametersUpdater updater;
    updater.AddParameter(&parameter);
    updater.Reset();

    // After Reset the updater is inactive: DateUpdate no longer touches the parameter.
    updater.DateUpdate(GetMJD(2010, 3, 1));
    EXPECT_EQ(parameter.GetValue(), -1);
}

TEST(SettingsModelMonthly, SetParameterMonthlyValuesAttachesModifier) {
    SettingsModel settings;
    settings.AddHydroUnitBrick("canopy", "storage");
    settings.AddBrickParameter("capacity", 2.0f);

    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    EXPECT_TRUE(settings.SetParameterMonthlyValues("canopy", "capacity", values));

    auto modified = settings.GetParametersWithModifier();
    ASSERT_EQ(modified.size(), 1);
    EXPECT_TRUE(modified[0]->HasModifier());
    // The scalar value is set to the annual mean baseline (6.5).
    EXPECT_FLOAT_EQ(modified[0]->GetValue(), 6.5f);

    // The modifier drives the value to the March entry when updated.
    modified[0]->UpdateFromModifier(GetMJD(2010, 3, 15));
    EXPECT_FLOAT_EQ(modified[0]->GetValue(), 3.0f);
}

TEST(SettingsModelMonthly, SetParameterMonthlyValuesWrongSize) {
    SettingsModel settings;
    settings.AddHydroUnitBrick("canopy", "storage");
    settings.AddBrickParameter("capacity", 2.0f);

    std::vector<float> values{1, 2, 3};
    EXPECT_FALSE(settings.SetParameterMonthlyValues("canopy", "capacity", values));
    EXPECT_TRUE(settings.GetParametersWithModifier().empty());
}

TEST(SettingsModelMonthly, SetParameterMonthlyValuesUnknownComponent) {
    SettingsModel settings;
    settings.AddHydroUnitBrick("canopy", "storage");
    settings.AddBrickParameter("capacity", 2.0f);

    std::vector<float> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    EXPECT_FALSE(settings.SetParameterMonthlyValues("missing", "capacity", values));
    EXPECT_TRUE(settings.GetParametersWithModifier().empty());
}

TEST(HydroUnitProperty, EmptyUnitReturnsStoredValue) {
    // A property stored in "mm" must return its value when queried with no unit (the
    // spatial-parameter population path reads properties in their native unit).
    HydroUnitProperty property("fc", 250.0, "mm");
    EXPECT_DOUBLE_EQ(property.GetValue(), 250.0);
    EXPECT_DOUBLE_EQ(property.GetValue("mm"), 250.0);
}

TEST(HydroUnitSpatial, ParameterOverrideStore) {
    HydroUnit unit;
    EXPECT_FALSE(unit.HasParameterOverride("soil:capacity"));

    unit.SetParameterOverride("soil:capacity", 123.0f);
    ASSERT_TRUE(unit.HasParameterOverride("soil:capacity"));
    EXPECT_FLOAT_EQ(*unit.GetParameterOverridePointer("soil:capacity"), 123.0f);

    // The pointer is stable and reflects updates in place (used live by the solver).
    const float* ptr = unit.GetParameterOverridePointer("soil:capacity");
    unit.SetParameterOverride("soil:capacity", 456.0f);
    EXPECT_FLOAT_EQ(*ptr, 456.0f);
}

TEST(SettingsModelSpatial, SpatialBindingRegistered) {
    SettingsModel settings;
    settings.SetParameterSpatialFromProperty("soil_moisture", "capacity", "fc");

    const auto& bindings = settings.GetSpatialParameterBindings();
    ASSERT_EQ(bindings.size(), 1);
    EXPECT_EQ(bindings.at({"soil_moisture", "capacity"}), "fc");

    // A comma-separated component list binds several components to the same property.
    settings.SetParameterSpatialFromProperty("a, b", "capacity", "cap");
    EXPECT_EQ(settings.GetSpatialParameterBindings().at({"a", "capacity"}), "cap");
    EXPECT_EQ(settings.GetSpatialParameterBindings().at({"b", "capacity"}), "cap");
}
