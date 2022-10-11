#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

/**
 * Model: model with glacier and surface components
 */

class GlacierSurfaceComponentModel : public ::testing::Test {
  protected:
    SettingsModel m_model;
    TimeSeriesUniform* m_tsPrecip{};
    TimeSeriesUniform* m_tsTemp{};

    void SetUp() override {
        m_model.SetSolver("HeunExplicit");
        m_model.SetTimer("2020-01-01", "2020-01-10", 1, "Day");

        // Surface elements
        m_model.AddSurfaceBrick("ground", "GenericSurface");
        m_model.AddSurfaceBrick("glacier", "Glacier");
        m_model.GeneratePrecipitationSplitters(true);
        m_model.GenerateSnowpacks("Melt:degree-day");
        m_model.GenerateSurfaceComponentBricks(true);
        m_model.GenerateSurfaceBricks();

        // Transfer rain to surface
        m_model.SelectHydroUnitBrick("ground");
        m_model.AddBrickProcess("outflow-rain", "Outflow:direct");
        m_model.AddProcessOutput("ground-surface");
        m_model.SelectHydroUnitBrick("glacier");
        m_model.AddBrickProcess("outflow-rain", "Outflow:direct");
        m_model.AddProcessOutput("glacier-surface");

        // Rain/snow splitter
        m_model.SelectHydroUnitSplitter("snow-rain-transition");
        m_model.AddSplitterParameter("transitionStart", 0.0f);
        m_model.AddSplitterParameter("transitionEnd", 2.0f);

        // Snowpack brick on surface 1
        m_model.SelectHydroUnitBrick("ground-snowpack");
        m_model.AddBrickLogging("snow");
        m_model.AddBrickLogging("content");

        // Snow melt process
        m_model.SelectProcess("melt");
        m_model.AddProcessParameter("degreeDayFactor", 3.0f);
        m_model.AddProcessParameter("meltingTemperature", 2.0f);
        m_model.AddProcessLogging("output");

        // Snowpack brick on surface 2
        m_model.SelectHydroUnitBrick("glacier-snowpack");
        m_model.AddBrickLogging("snow");
        m_model.AddBrickLogging("content");

        // Snow melt process
        m_model.SelectProcess("melt");
        m_model.AddProcessParameter("degreeDayFactor", 3.0f);
        m_model.AddProcessParameter("meltingTemperature", 2.0f);
        m_model.AddProcessLogging("output");

        // Glacier melt process
        m_model.SelectHydroUnitBrick("glacier");
        m_model.AddBrickParameter("noMeltWhenSnowCover", 1.0);
        m_model.AddBrickParameter("infiniteStorage", 1.0);
        m_model.AddBrickProcess("melt", "Melt:degree-day");
        m_model.AddProcessForcing("Temperature");
        m_model.AddProcessParameter("degreeDayFactor", 4.0f);
        m_model.AddProcessParameter("meltingTemperature", 1.0f);
        m_model.AddProcessLogging("output");
        m_model.AddProcessOutput("glacier-surface");

        // Surface brick for the bare ground with a direct outflow
        m_model.SelectHydroUnitBrick("ground-surface");
        m_model.AddBrickProcess("outflow", "Outflow:direct");
        m_model.AddProcessLogging("output");
        m_model.AddProcessOutput("outlet");

        // Surface brick for the glacier part with a direct outflow
        m_model.SelectHydroUnitBrick("glacier-surface");
        m_model.AddBrickProcess("outflow", "Outflow:direct");
        m_model.AddProcessLogging("output");
        m_model.AddProcessOutput("outlet");

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

TEST_F(GlacierSurfaceComponentModel, HandlesPartialGlacierCoverWithSnowpack) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddSurfaceElement("ground", 0.5);
    basinSettings.AddSurfaceElement("glacier", 0.5);

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

    vecDouble expectedNull = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    vecDouble expectedSWE = {0.0, 10.0, 20.0, 25.0, 25.0, 22.0, 16.0, 7.0, 0.0, 0.0};
    vecDouble expectedSnowMelt = {0.0, 0.0, 0.0, 0.0, 0.0, 3.0, 6.0, 9.0, 7.0, 0.0};
    vecDouble expectedIceMelt = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 28.0, 32.0};
    vecDouble expectedTotGlacierContrib = {0.0, 0.0, 0.0, 0.0, 0.0, 3.0, 6.0, 9.0, 35.0, 32.0};

    for (int j = 0; j < expectedSWE.size(); ++j) {
        EXPECT_NEAR(unitContent[0](j, 0), expectedSWE[j], 0.000001);       // ground-snowpack-snow
        EXPECT_NEAR(unitContent[1](j, 0), expectedNull[j], 0.000001);      // ground-snowpack-content (water)
        EXPECT_NEAR(unitContent[2](j, 0), expectedSnowMelt[j], 0.000001);  // ground-snowmelt
        EXPECT_NEAR(unitContent[3](j, 0), expectedSWE[j], 0.000001);       // glacier-snowpack-snow
        EXPECT_NEAR(unitContent[4](j, 0), expectedNull[j], 0.000001);      // glacier-snowpack-content (water)
        EXPECT_NEAR(unitContent[5](j, 0), expectedSnowMelt[j], 0.000001);  // glacier-snowmelt
        EXPECT_NEAR(unitContent[6](j, 0), expectedIceMelt[j], 0.000001);   // glacier-icemelt
    }
}
