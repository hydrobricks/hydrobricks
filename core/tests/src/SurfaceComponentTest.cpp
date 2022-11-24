#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

/**
 * Model: model with glacier and land cover components
 */

class GlacierComponentModel : public ::testing::Test {
  protected:
    SettingsModel m_model;
    TimeSeriesUniform* m_tsPrecip{};
    TimeSeriesUniform* m_tsTemp{};

    void SetUp() override {
        m_model.SetSolver("heun_explicit");
        m_model.SetTimer("2020-01-01", "2020-01-10", 1, "day");
        m_model.SetLogAll(true);

        // Precipitation
        m_model.GeneratePrecipitationSplitters(true);

        // Land cover elements
        m_model.AddLandCoverBrick("ground", "generic_land_cover");
        m_model.AddLandCoverBrick("glacier", "glacier");
        m_model.GenerateSnowpacks("melt:degree_day");

        // Rain/snow splitter
        m_model.SelectHydroUnitSplitter("snow_rain_transition");
        m_model.AddSplitterParameter("transition_start", 0.0f);
        m_model.AddSplitterParameter("transition_end", 2.0f);

        // Snow melt process on ground
        m_model.SelectHydroUnitBrick("ground-snowpack");
        m_model.SelectProcess("melt");
        m_model.AddProcessParameter("degree_day_factor", 3.0f);
        m_model.AddProcessParameter("melting_temperature", 2.0f);

        // Snow melt process on glacier
        m_model.SelectHydroUnitBrick("glacier-snowpack");
        m_model.SelectProcess("melt");
        m_model.AddProcessParameter("degree_day_factor", 3.0f);
        m_model.AddProcessParameter("melting_temperature", 2.0f);

        // Glacier melt process
        m_model.SelectHydroUnitBrick("glacier");
        m_model.AddBrickParameter("no_melt_when_snow_cover", 1.0);
        m_model.AddBrickParameter("infinite_storage", 1.0);
        m_model.AddBrickProcess("melt", "melt:degree_day", "glacier");
        m_model.AddProcessForcing("temperature");
        m_model.AddProcessParameter("degree_day_factor", 4.0f);
        m_model.AddProcessParameter("melting_temperature", 1.0f);
        m_model.SetProcessOutputsAsInstantaneous();
        m_model.AddBrickProcess("outflow", "outflow:direct", "outlet");
        m_model.SetProcessOutputsAsInstantaneous();

        // Land cover brick for the bare ground with a direct outflow
        m_model.SelectHydroUnitBrick("ground");
        m_model.AddBrickProcess("outflow", "outflow:direct", "outlet");

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

TEST_F(GlacierComponentModel, HandlesPartialGlacierCoverWithSnowpack) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model));
    EXPECT_TRUE(model.IsOk());

    EXPECT_TRUE(subBasin.AssignFractions(basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble expectedOutputs = {0.0, 0.0, 0.0, 5.0, 10.0, 13.0, 16.0, 19.0, 31.0, 16.0};

    for (auto& basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }

    // Check components
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    // [0] "ground:content"
    // [1] "ground:outflow:output"
    // [2] "glacier:content"
    // [3] "glacier:melt:output"
    // [4] "glacier:outflow:output"
    // [5] "ground-snowpack:content"
    // [6] "ground-snowpack:snow"
    // [7] "ground-snowpack:melt:output"
    // [8] "glacier-snowpack:content"
    // [9] "glacier-snowpack:snow"
    // [10] "glacier-snowpack:melt:output"

    vecDouble expectedNull = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    vecDouble expectedSWE = {0.0, 10.0, 20.0, 25.0, 25.0, 22.0, 16.0, 7.0, 0.0, 0.0};
    vecDouble expectedSnowMelt = {0.0, 0.0, 0.0, 0.0, 0.0, 3.0, 6.0, 9.0, 7.0, 0.0};
    vecDouble expectedIceMelt = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 28.0, 32.0};
    // Contributions fractionated:
    vecDouble expectedGroundOut = {0.0, 0.0, 0.0, 2.5, 5.0, 6.5, 8.0, 9.5, 8.5, 0.0};
    vecDouble expectedGlacierOut = {0.0, 0.0, 0.0, 2.5, 5.0, 6.5, 8.0, 9.5, 22.5, 16.0};

    for (int j = 0; j < expectedSWE.size(); ++j) {
        EXPECT_NEAR(unitContent[0](j, 0), expectedNull[j], 0.000001);        // ground:content (direct flow)
        EXPECT_NEAR(unitContent[1](j, 0), expectedGroundOut[j], 0.000001);   // ground:outflow:output
        EXPECT_NEAR(unitContent[2](j, 0), expectedNull[j], 0.000001);        // glacier:content (direct flow)
        EXPECT_NEAR(unitContent[3](j, 0), expectedIceMelt[j], 0.000001);     // glacier:melt:output
        EXPECT_NEAR(unitContent[4](j, 0), expectedGlacierOut[j], 0.000001);  // glacier:outflow:output
        EXPECT_NEAR(unitContent[5](j, 0), expectedNull[j], 0.000001);        // ground-snowpack:content (water)
        EXPECT_NEAR(unitContent[6](j, 0), expectedSWE[j], 0.000001);         // ground-snowpack:snow
        EXPECT_NEAR(unitContent[7](j, 0), expectedSnowMelt[j], 0.000001);    // ground-snowpack:melt:output
        EXPECT_NEAR(unitContent[8](j, 0), expectedNull[j], 0.000001);        // glacier-snowpack:content (water)
        EXPECT_NEAR(unitContent[9](j, 0), expectedSWE[j], 0.000001);         // glacier-snowpack:snow
        EXPECT_NEAR(unitContent[10](j, 0), expectedSnowMelt[j], 0.000001);   // glacier-snowpack:melt:output
    }
}
