#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

/**
 * Model: simple snow and ice model
 */

class SnowIceModel : public ::testing::Test {
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

        // Add default ground land cover
        m_model.AddLandCoverBrick("ground", "ground");
        m_model.AddLandCoverBrick("glacier", "glacier");

        // Snowpacks
        m_model.GenerateSnowpacks("melt:degree_day");
        m_model.SelectHydroUnitBrick("glacier_snowpack");
        m_model.SelectProcess("melt");
        m_model.SetProcessParameterValue("degree_day_factor", 3.0f);
        m_model.SetProcessParameterValue("melting_temperature", 2.0f);
        m_model.SelectHydroUnitBrick("ground_snowpack");
        m_model.SelectProcess("melt");
        m_model.SetProcessParameterValue("degree_day_factor", 3.0f);
        m_model.SetProcessParameterValue("melting_temperature", 2.0f);

        // Ice melt
        m_model.SelectHydroUnitBrick("glacier");
        m_model.AddBrickProcess("melt", "melt:degree_day", "glacier:water");
        m_model.SetProcessParameterValue("degree_day_factor", 3.0f);
        m_model.SetProcessParameterValue("melting_temperature", 2.0f);

        // Add transformation of snow to ice
        m_model.SelectHydroUnitBrick("glacier_snowpack");
        m_model.AddBrickProcess("snow_ice_transfo", "transform:snow_to_ice", "glacier:ice");
        m_model.SetProcessParameterValue("snow_ice_transformation_rate", 0.002f);

        // Add process to direct meltwater to the outlet
        m_model.SelectHydroUnitBrick("ground_snowpack");
        m_model.AddBrickProcess("meltwater", "outflow:direct");
        m_model.AddProcessOutput("outlet");
        m_model.SelectHydroUnitBrick("glacier_snowpack");
        m_model.AddBrickProcess("meltwater", "outflow:direct");
        m_model.AddProcessOutput("outlet");

        // Add process to direct water to the outlet
        m_model.SelectHydroUnitBrick("ground");
        m_model.AddBrickProcess("outflow", "outflow:direct");
        m_model.AddProcessOutput("outlet");
        m_model.SelectHydroUnitBrick("glacier");
        m_model.AddBrickProcess("outflow", "outflow:direct");
        m_model.AddProcessOutput("outlet");

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

TEST_F(SnowIceModel, SnowIceTransformation) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
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
    vecDouble glacierContent = {100.0, 100.002, 100.004, 100.006, 100.008, 100.010 - 3, 100.012 - 9,
                                100.014 - 18, 100.014 - 36, 100.014 - 57};
    vecDouble expectedWaterRetention = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    vecDouble expectedSWEGround = {0.0, 10.0, 20.0, 25.0, 25.0, 22.0, 16.0, 7.0, 0.0, 0.0};
    vecDouble expectedSWEGlacier = {0.0, 10.0 - 0.002, 20.0 - 0.004, 25.0 - 0.006, 25.0 - 0.008,
                                    22.0 - 0.010, 16.0 - 0.012, 7.0 - 0.014, 0.0, 0.0};
    vecDouble snowIceTransfo = {0.0, 0.002, 0.002, 0.002, 0.002, 0.002, 0.002, 0.002, 0.0, 0.0};

    for (int j = 0; j < expectedSWEGround.size(); ++j) {
        // Melt processes
        EXPECT_NEAR(unitContent[8](j, 0), expectedSnowMeltGround[j], 0.000001);  // Ground snowpack
        EXPECT_NEAR(unitContent[12](j, 0), expectedSnowMeltIce[j], 0.000001);  // Glacier snowpack
        EXPECT_NEAR(unitContent[4](j, 0), expectedIceMelt[j], 0.000001);  // Glacier ice melt
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
