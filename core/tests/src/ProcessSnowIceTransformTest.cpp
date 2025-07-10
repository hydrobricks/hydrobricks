#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

/**
 * Model: simple snow and ice model
 */

class SnowIceModel : public ::testing::Test {
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

        // Add default ground land cover
        _model.AddLandCoverBrick("ground", "ground");
        _model.AddLandCoverBrick("glacier", "glacier");

        // Snowpacks
        _model.GenerateSnowpacks("melt:degree_day");
        _model.AddSnowIceTransformation("transform:snow_ice_constant");
        _model.SetParameterValue("glacier_snowpack", "snow_ice_transformation_rate", 0.002f);
        _model.SelectHydroUnitBrick("glacier_snowpack");
        _model.SelectProcess("melt");
        _model.SetProcessParameterValue("degree_day_factor", 3.0f);
        _model.SetProcessParameterValue("melting_temperature", 2.0f);
        _model.SelectHydroUnitBrick("ground_snowpack");
        _model.SelectProcess("melt");
        _model.SetProcessParameterValue("degree_day_factor", 3.0f);
        _model.SetProcessParameterValue("melting_temperature", 2.0f);

        // Ice melt
        _model.SelectHydroUnitBrick("glacier");
        _model.AddBrickProcess("melt", "melt:degree_day", "glacier:water");
        _model.SetProcessParameterValue("degree_day_factor", 3.0f);
        _model.SetProcessParameterValue("melting_temperature", 2.0f);

        // Add process to direct meltwater to the outlet
        _model.SelectHydroUnitBrick("ground_snowpack");
        _model.AddBrickProcess("meltwater", "outflow:direct");
        _model.AddProcessOutput("outlet");
        _model.SelectHydroUnitBrick("glacier_snowpack");
        _model.AddBrickProcess("meltwater", "outflow:direct");
        _model.AddProcessOutput("outlet");

        // Add process to direct water to the outlet
        _model.SelectHydroUnitBrick("ground");
        _model.AddBrickProcess("outflow", "outflow:direct");
        _model.AddProcessOutput("outlet");
        _model.SelectHydroUnitBrick("glacier");
        _model.AddBrickProcess("outflow", "outflow:direct");
        _model.AddProcessOutput("outlet");

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

TEST_F(SnowIceModel, SnowIceTransformation) {
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

    // Set initial glacier content
    model.GetSubBasin()->GetHydroUnit(0)->GetBrick("glacier")->UpdateContent(100, "ice");

    EXPECT_TRUE(model.Run());

    // Check model values
    Logger* logger = model.GetLogger();
    vecAxxd unitContent = logger->GetHydroUnitValues();

    vecDouble expectedSnowMeltGround = {0.0, 0.0, 0.0, 0.0, 0.0, 3.0, 6.0, 9.0, 7.0, 0.0};
    vecDouble expectedSnowMeltIce = {0.0, 0.0, 0.0, 0.0, 0.0, 3.0, 6.0, 9.0, 7.0 - 0.014, 0.0};
    vecDouble expectedIceMelt = {0.0, 0.0, 0.0, 0.0, 0.0, 3.0, 6.0, 9.0, 18.0, 21.0};
    vecDouble glacierContent = {100.0,       100.002,     100.004,      100.006,      100.008,
                                100.010 - 3, 100.012 - 9, 100.014 - 18, 100.014 - 36, 100.014 - 57};
    vecDouble expectedWaterRetention = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    vecDouble expectedSWEGround = {0.0, 10.0, 20.0, 25.0, 25.0, 22.0, 16.0, 7.0, 0.0, 0.0};
    vecDouble expectedSWEGlacier = {0.0,          10.0 - 0.002, 20.0 - 0.004, 25.0 - 0.006, 25.0 - 0.008,
                                    22.0 - 0.010, 16.0 - 0.012, 7.0 - 0.014,  0.0,          0.0};
    vecDouble snowIceTransfo = {0.0, 0.002, 0.002, 0.002, 0.002, 0.002, 0.002, 0.002, 0.0, 0.0};

    for (int j = 0; j < expectedSWEGround.size(); ++j) {
        // Melt processes
        EXPECT_NEAR(unitContent[8](j, 0), expectedSnowMeltGround[j], 0.000001);  // Ground snowpack
        EXPECT_NEAR(unitContent[12](j, 0), expectedSnowMeltIce[j], 0.000001);    // Glacier snowpack
        EXPECT_NEAR(unitContent[4](j, 0), expectedIceMelt[j], 0.000001);         // Glacier ice melt
        // Ice content
        EXPECT_NEAR(unitContent[3](j, 0), glacierContent[j], 0.000001);
        // Ground snowpack
        EXPECT_NEAR(unitContent[6](j, 0), expectedWaterRetention[j], 0.000001);
        EXPECT_NEAR(unitContent[7](j, 0), expectedSWEGround[j], 0.000001);
        // Glacier snowpack
        EXPECT_NEAR(unitContent[10](j, 0), expectedWaterRetention[j], 0.000001);
        EXPECT_NEAR(unitContent[11](j, 0), expectedSWEGlacier[j], 0.000001);
        // Snow to ice transformation
        EXPECT_NEAR(unitContent[13](j, 0), snowIceTransfo[j], 0.000001);
    }
}

TEST_F(SnowIceModel, SnowIceTransformationWithNullFraction) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);
    basinSettings.AddLandCover("glacier", "", 0.0);
    basinSettings.AddHydroUnit(2, 100);
    basinSettings.AddLandCover("ground", "", 1.0);
    basinSettings.AddLandCover("glacier", "", 0.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(_tsTemp));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    // Set initial glacier content
    model.GetSubBasin()->GetHydroUnit(0)->GetBrick("glacier")->UpdateContent(100, "ice");

    EXPECT_TRUE(model.Run());
}