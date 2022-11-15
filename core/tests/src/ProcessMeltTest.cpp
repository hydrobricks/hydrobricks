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

    void SetUp() override {
        m_model.SetSolver("HeunExplicit");
        m_model.SetTimer("2020-01-01", "2020-01-10", 1, "Day");

        // Snowpack brick
        m_model.AddHydroUnitBrick("snowpack", "Snowpack");
        m_model.AddBrickLogging({"content", "snow"});

        // Snow melt process
        m_model.AddBrickProcess("melt", "Melt:degree-day");
        m_model.AddProcessForcing("Temperature");
        m_model.AddProcessParameter("degreeDayFactor", 3.0f);
        m_model.AddProcessParameter("meltingTemperature", 2.0f);
        m_model.OutputProcessToSameBrick();
        m_model.AddProcessLogging("output");

        // Add process to direct meltwater to the outlet
        m_model.AddBrickProcess("meltwater", "Outflow:direct");
        m_model.AddProcessOutput("outlet");
        m_model.AddProcessLogging("output");

        // Rain/snow splitter
        m_model.AddHydroUnitSplitter("snow-rain", "SnowRain");
        m_model.AddSplitterForcing("Precipitation");
        m_model.AddSplitterForcing("Temperature");
        m_model.AddSplitterOutput("outlet");            // rain
        m_model.AddSplitterOutput("snowpack", "snow");  // snow
        m_model.AddSplitterParameter("transitionStart", 0.0f);
        m_model.AddSplitterParameter("transitionEnd", 2.0f);

        m_model.AddLoggingToItem("outlet");

        auto precip = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0});
        m_tsPrecip = new TimeSeriesUniform(Precipitation);
        m_tsPrecip->SetData(precip);

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        temperature->SetValues({-2.0, -1.0, -1.0, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 9.0});
        m_tsTemp = new TimeSeriesUniform(Temperature);
        m_tsTemp->SetData(temperature);
    }
    void TearDown() override {
        wxDELETE(m_tsPrecip);
        wxDELETE(m_tsTemp);
    }
};
/*
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
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble expectedOutputs = {0.0, 0.0, 0.0, 5.0, 10.0, 13.0, 16.0, 19.0, 17.0, 0.0};

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
*/
class GlacierModel : public ::testing::Test {
  protected:
    SettingsModel m_model;
    TimeSeriesUniform* m_tsTemp{};

    void SetUp() override {
        m_model.SetSolver("HeunExplicit");
        m_model.SetTimer("2020-01-01", "2020-01-10", 1, "Day");

        // Glacier brick
        m_model.AddHydroUnitBrick("glacier", "Glacier");

        // Glacier melt process
        m_model.AddBrickProcess("melt", "Melt:degree-day");
        m_model.AddProcessForcing("Temperature");
        m_model.AddProcessParameter("degreeDayFactor", 3.0f);
        m_model.AddProcessParameter("meltingTemperature", 2.0f);
        m_model.AddProcessLogging("output");
        m_model.AddProcessOutput("outlet");

        m_model.AddLoggingToItem("outlet");

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        temperature->SetValues({-2.0, -1.0, -1.0, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 9.0});
        m_tsTemp = new TimeSeriesUniform(Temperature);
        m_tsTemp->SetData(temperature);
    }
    void TearDown() override {
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
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble expectedOutputs = {0.0, 0.0, 0.0, 0.0, 0.0, 3.0, 6.0, 9.0, 18.0, 21.0};

    for (auto& basinOutput : basinOutputs) {
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

class GlacierModelWithSnowpack : public ::testing::Test {
  protected:
    SettingsModel m_model;
    TimeSeriesUniform* m_tsTemp{};
    TimeSeriesUniform* m_tsPrecip{};

    void SetUp() override {
        m_model.SetSolver("HeunExplicit");
        m_model.SetTimer("2020-01-01", "2020-01-08", 1, "Day");

        // Precipitation
        m_model.GeneratePrecipitationSplitters(true);

        // Glacier brick
        m_model.AddLandCoverBrick("glacier", "Glacier");

        // Snowpack brick
        m_model.GenerateSnowpacks("Melt:degree-day");

        // Snow melt process
        m_model.SelectHydroUnitBrick("glacier-snowpack");
        m_model.AddProcessParameter("degreeDayFactor", 2.0f);
        m_model.AddProcessParameter("meltingTemperature", 0.0f);
        m_model.AddProcessLogging("output");

        // Glacier melt process
        m_model.SelectHydroUnitBrick("glacier");
        m_model.AddBrickProcess("melt", "Melt:degree-day", "outlet");
        m_model.AddBrickParameter("noMeltWhenSnowCover", true);
        m_model.AddBrickParameter("infiniteStorage", 1.0);
        m_model.AddProcessForcing("Temperature");
        m_model.AddProcessParameter("degreeDayFactor", 3.0f);
        m_model.AddProcessParameter("meltingTemperature", 0.0f);
        m_model.AddProcessLogging("output");

        // Add process to direct meltwater to the outlet
        m_model.AddBrickProcess("meltwater", "Outflow:direct", "outlet", true);

        m_model.AddLoggingToItem("outlet");

        auto precip = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 8), 1, Day);
        precip->SetValues({8.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        m_tsPrecip = new TimeSeriesUniform(Precipitation);
        m_tsPrecip->SetData(precip);

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 8), 1, Day);
        temperature->SetValues({-2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0});
        m_tsTemp = new TimeSeriesUniform(Temperature);
        m_tsTemp->SetData(temperature);
    }
    void TearDown() override {
        wxDELETE(m_tsTemp);
        wxDELETE(m_tsPrecip);
    }
};

TEST_F(GlacierModelWithSnowpack, NoIceMeltIfSnowCover) {
    wxLogNull logNo;

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
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble expectedOutputs = {0.0, 4.0, 10.0, 6.0, 6.0, 6.0, 6.0, 6.0};

    for (auto& basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            // EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
            wxPrintf(" | %.1f (%.1f)", basinOutput[j], expectedOutputs[j]);
        }
    }

    wxPrintf("\n");

    // Check melt and swe
    vecAxxd unitOutput = model.GetLogger()->GetHydroUnitValues();

    vecDouble expectedSnowMelt = {0.0, 4.0, 4.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    vecDouble expectedIceMelt = {0.0, 0.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0};
    vecDouble expectedTotMeltWater = {0.0, 4.0, 10.0, 6.0, 6.0, 6.0, 6.0, 6.0};

    for (int j = 0; j < expectedIceMelt.size(); ++j) {
        // EXPECT_NEAR(unitOutput[0](j, 0), expectedSnowMelt[j], 0.000001);
        // EXPECT_NEAR(unitOutput[1](j, 0), expectedIceMelt[j], 0.000001);
        // EXPECT_NEAR(unitOutput[2](j, 0), expectedTotMeltWater[j], 0.000001);

        wxPrintf(" | %.1f (%.1f)", unitOutput[0](j, 0), expectedSnowMelt[j]);
    }
    wxPrintf("\n");

    for (int j = 0; j < expectedIceMelt.size(); ++j) {
        // EXPECT_NEAR(unitOutput[0](j, 0), expectedSnowMelt[j], 0.000001);
        // EXPECT_NEAR(unitOutput[1](j, 0), expectedIceMelt[j], 0.000001);
        // EXPECT_NEAR(unitOutput[2](j, 0), expectedTotMeltWater[j], 0.000001);

        wxPrintf(" | %.1f (%.1f)", unitOutput[1](j, 0), expectedIceMelt[j]);
    }
    wxPrintf("\n");

    for (int j = 0; j < expectedIceMelt.size(); ++j) {
        // EXPECT_NEAR(unitOutput[0](j, 0), expectedSnowMelt[j], 0.000001);
        // EXPECT_NEAR(unitOutput[1](j, 0), expectedIceMelt[j], 0.000001);
        // EXPECT_NEAR(unitOutput[2](j, 0), expectedTotMeltWater[j], 0.000001);

        wxPrintf(" | %.1f (%.1f)", unitOutput[2](j, 0), expectedTotMeltWater[j]);
    }
    wxPrintf("\n");
}
