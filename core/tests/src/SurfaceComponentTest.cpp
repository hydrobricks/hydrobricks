#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

/**
 * Model: model with glacier and land cover components
 */

class GlacierComponentModel : public ::testing::Test {
  protected:
    SettingsModel _model;
    TimeSeriesUniform* _tsPrecip{};
    TimeSeriesUniform* _tsTemp{};

    void SetUp() override {
        _model.SetSolver("heun_explicit");
        _model.SetTimer("2020-01-01", "2020-01-10", 1, "day");
        _model.SetLogAll(true);

        // Precipitation
        _model.GeneratePrecipitationSplitters(true);

        // Land cover elements
        _model.AddLandCoverBrick("ground", "generic_land_cover");
        _model.AddLandCoverBrick("glacier", "glacier");
        _model.GenerateSnowpacks("melt:degree_day");

        // Rain/snow splitter
        _model.SelectHydroUnitSplitter("snow_rain_transition");
        _model.AddSplitterParameter("transition_start", 0.0f);
        _model.AddSplitterParameter("transition_end", 2.0f);

        // Snow melt process on ground
        _model.SelectHydroUnitBrick("ground_snowpack");
        _model.SelectProcess("melt");
        _model.SetProcessParameterValue("degree_day_factor", 3.0f);
        _model.SetProcessParameterValue("melting_temperature", 2.0f);

        // Snow melt process on glacier
        _model.SelectHydroUnitBrick("glacier_snowpack");
        _model.SelectProcess("melt");
        _model.SetProcessParameterValue("degree_day_factor", 3.0f);
        _model.SetProcessParameterValue("melting_temperature", 2.0f);

        // Glacier melt process
        _model.SelectHydroUnitBrick("glacier");
        _model.AddBrickParameter("no_melt_when_snow_cover", 1.0);
        _model.AddBrickParameter("infinite_storage", 1.0);
        _model.AddBrickProcess("melt", "melt:degree_day", "glacier");
        _model.SetProcessParameterValue("degree_day_factor", 4.0f);
        _model.SetProcessParameterValue("melting_temperature", 1.0f);
        _model.SetProcessOutputsAsInstantaneous();
        _model.AddBrickProcess("outflow", "outflow:direct", "outlet");
        _model.SetProcessOutputsAsInstantaneous();

        // Land cover brick for the bare ground with a direct outflow
        _model.SelectHydroUnitBrick("ground");
        _model.AddBrickProcess("outflow", "outflow:direct", "outlet");

        _model.AddLoggingToItem("outlet");

        auto precip = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0});
        _tsPrecip = new TimeSeriesUniform(Precipitation);
        _tsPrecip->SetData(precip);

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        temperature->SetValues({-2.0, -1.0, -1.0, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 9.0});
        _tsTemp = new TimeSeriesUniform(Temperature);
        _tsTemp->SetData(temperature);
    }
    void TearDown() override {
        wxDELETE(_tsPrecip);
        wxDELETE(_tsTemp);
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
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(_tsTemp));
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
    // [0] "ground:water_content"
    // [1] "ground:outflow:output"
    // [2] "glacier:water_content"
    // [3] "glacier:ice_content"
    // [4] "glacier:melt:output"
    // [5] "glacier:outflow:output"
    // [6] "ground_snowpack:water_content"
    // [7] "ground_snowpack:snow_content"
    // [8] "ground_snowpack:melt:output"
    // [9] "glacier_snowpack:water_content"
    // [10] "glacier_snowpack:snow_content"
    // [11] "glacier_snowpack:melt:output"

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
        EXPECT_NEAR(unitContent[4](j, 0), expectedIceMelt[j], 0.000001);     // glacier:melt:output
        EXPECT_NEAR(unitContent[5](j, 0), expectedGlacierOut[j], 0.000001);  // glacier:outflow:output
        EXPECT_NEAR(unitContent[6](j, 0), expectedNull[j], 0.000001);        // ground_snowpack:content (water)
        EXPECT_NEAR(unitContent[7](j, 0), expectedSWE[j], 0.000001);         // ground_snowpack:snow
        EXPECT_NEAR(unitContent[8](j, 0), expectedSnowMelt[j], 0.000001);    // ground_snowpack:melt:output
        EXPECT_NEAR(unitContent[9](j, 0), expectedNull[j], 0.000001);        // glacier_snowpack:content (water)
        EXPECT_NEAR(unitContent[10](j, 0), expectedSWE[j], 0.000001);        // glacier_snowpack:snow
        EXPECT_NEAR(unitContent[11](j, 0), expectedSnowMelt[j], 0.000001);   // glacier_snowpack:melt:output
    }
}
