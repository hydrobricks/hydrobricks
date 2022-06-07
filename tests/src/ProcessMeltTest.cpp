#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

/**
 * Model: simple snowpack model
 */

class SnowpackModel : public ::testing::Test {
  protected:
    SettingsModel m_model;
    TimeSeriesUniform* m_tsPrecip{};
    TimeSeriesUniform* m_tsTemp{};

    virtual void SetUp() {
        m_model.SetSolver("HeunExplicit");
        m_model.SetTimer("2020-01-01", "2020-01-10", 1, "Day");

        // Snowpack brick
        m_model.AddBrick("snowpack", "Snowpack");
        m_model.AddLoggingToCurrentBrick("content");
        m_model.AddLoggingToCurrentBrick("snow");

        // Snow melt process
        m_model.AddProcessToCurrentBrick("melt", "Melt:degree-day");
        m_model.AddForcingToCurrentProcess("Temperature");
        m_model.AddParameterToCurrentProcess("degreeDayFactor", 3.0f);
        m_model.AddParameterToCurrentProcess("meltingTemperature", 2.0f);
        m_model.AddOutputToCurrentProcess("snowpack");
        m_model.AddLoggingToCurrentProcess("output");

        // Add process to direct meltwater to the outlet
        m_model.AddProcessToCurrentBrick("meltwater", "Outflow:direct");
        m_model.AddOutputToCurrentProcess("outlet");
        m_model.AddLoggingToCurrentProcess("output");

        // Rain/snow splitter
        m_model.AddSplitter("snow-rain", "SnowRain");
        m_model.AddForcingToCurrentSplitter("Precipitation");
        m_model.AddForcingToCurrentSplitter("Temperature");
        m_model.AddOutputToCurrentSplitter("outlet"); // rain
        m_model.AddOutputToCurrentSplitter("snowpack", "snow"); // snow
        m_model.AddParameterToCurrentSplitter("transitionStart", 0.0f);
        m_model.AddParameterToCurrentSplitter("transitionEnd", 2.0f);

        m_model.AddLoggingToItem("outlet");

        auto precip = new TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                                wxDateTime(10, wxDateTime::Jan, 2020), 1, Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0});
        m_tsPrecip = new TimeSeriesUniform(Precipitation);
        m_tsPrecip->SetData(precip);

        auto temperature = new TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                                     wxDateTime(10, wxDateTime::Jan, 2020), 1, Day);
        temperature->SetValues({-2.0, -1.0, -1.0, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 9.0});
        m_tsTemp = new TimeSeriesUniform(Temperature);
        m_tsTemp->SetData(temperature);
    }
    virtual void TearDown() {
        wxDELETE(m_tsPrecip);
        wxDELETE(m_tsTemp);
    }
};

TEST_F(SnowpackModel, DegreeDay) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge
    vecAxd basinOutputs = model.GetLogger()->GetAggregatedValues();

    vecDouble expectedOutputs = {0.0, 0.0, 0.0, 5.0, 10.0, 10.0, 13.0, 16.0, 19.0, 7.0};

    for (auto & basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }

    // Check melt and swe
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();

    vecDouble expectedWaterRetention = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    vecDouble expectedSWE = {0.0, 10.0, 20.0, 25.0, 25.0, 22.0, 16.0, 7.0, 0.0, 0.0};
    vecDouble expectedMelt = {0.0, 0.0, 0.0, 0.0, 0.0, 3.0, 6.0, 9.0, 7.0, 0.0};

    for (int j = 0; j < expectedSWE.size(); ++j) {
        EXPECT_NEAR(unitContent[0](j, 0), expectedWaterRetention[j], 0.000001);
        EXPECT_NEAR(unitContent[1](j, 0), expectedSWE[j], 0.000001);
        EXPECT_NEAR(unitContent[2](j, 0), expectedMelt[j], 0.000001);
    }
}

class GlacierModel : public ::testing::Test {
  protected:
    SettingsModel m_model;
    TimeSeriesUniform* m_tsTemp{};

    virtual void SetUp() {
        m_model.SetSolver("HeunExplicit");
        m_model.SetTimer("2020-01-01", "2020-01-10", 1, "Day");

        // Glacier brick
        m_model.AddBrick("glacier", "Glacier");

        // Glacier melt process
        m_model.AddProcessToCurrentBrick("melt", "Melt:degree-day");
        m_model.AddForcingToCurrentProcess("Temperature");
        m_model.AddParameterToCurrentProcess("degreeDayFactor", 3.0f);
        m_model.AddParameterToCurrentProcess("meltingTemperature", 2.0f);
        m_model.AddLoggingToCurrentProcess("output");
        m_model.AddOutputToCurrentProcess("outlet");

        m_model.AddLoggingToItem("outlet");

        auto temperature = new TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                                     wxDateTime(10, wxDateTime::Jan, 2020), 1, Day);
        temperature->SetValues({-2.0, -1.0, -1.0, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 9.0});
        m_tsTemp = new TimeSeriesUniform(Temperature);
        m_tsTemp->SetData(temperature);
    }
    virtual void TearDown() {
        wxDELETE(m_tsTemp);
    }
};

TEST_F(GlacierModel, UnlimitedSupply) {
    wxLogNull logNo;

    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge
    vecAxd basinOutputs = model.GetLogger()->GetAggregatedValues();

    vecDouble expectedOutputs = {0.0, 0.0, 0.0, 0.0, 0.0, 3.0, 6.0, 9.0, 18.0, 21.0};

    for (auto & basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }

    // Check melt and swe
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();

    vecDouble expectedMelt = {0.0, 0.0, 0.0, 0.0, 0.0, 3.0, 6.0, 9.0, 18.0, 21.0};

    for (int j = 0; j < expectedMelt.size(); ++j) {
        EXPECT_NEAR(unitContent[0](j, 0), expectedMelt[j], 0.000001);
    }
}