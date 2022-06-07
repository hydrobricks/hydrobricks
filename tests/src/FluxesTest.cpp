#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "SettingsBasin.h"
#include "TimeSeriesUniform.h"

/**
 * Model: simple snowpack model
 */

class FluxWeightedModel : public ::testing::Test {
  protected:
    SettingsModel m_model;
    TimeSeriesUniform* m_tsPrecip{};

    virtual void SetUp() {
        m_model.SetSolver("HeunExplicit");
        m_model.SetTimer("2020-01-01", "2020-01-10", 1, "Day");

        // Surface elements
        m_model.AddSurfaceBrick("item-1", "GenericSurface");
        m_model.AddSurfaceBrick("item-2", "GenericSurface");
        m_model.GeneratePrecipitationSplitters(false);
        m_model.GenerateSurfaceComponentBricks(false);
        m_model.GenerateSurfaceBricks();

        // Direct outflow processes
        m_model.SelectBrick("item-1");
        m_model.AddProcessToCurrentBrick("outflow", "Outflow:direct");
        m_model.AddLoggingToCurrentProcess("output");
        m_model.AddOutputToCurrentProcess("item-1-surface");
        m_model.SelectBrick("item-2");
        m_model.AddProcessToCurrentBrick("outflow", "Outflow:direct");
        m_model.AddLoggingToCurrentProcess("output");
        m_model.AddOutputToCurrentProcess("item-2-surface");

        // Surface bricks
        m_model.SelectBrick("item-1-surface");
        m_model.AddProcessToCurrentBrick("outflow", "Outflow:direct");
        m_model.AddLoggingToCurrentProcess("output");
        m_model.AddOutputToCurrentProcess("outlet", true);
        m_model.SelectBrick("item-2-surface");
        m_model.AddProcessToCurrentBrick("outflow", "Outflow:direct");
        m_model.AddLoggingToCurrentProcess("output");
        m_model.AddOutputToCurrentProcess("outlet", true);

        m_model.AddLoggingToItem("outlet");

        auto precip = new TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                                wxDateTime(10, wxDateTime::Jan, 2020), 1, Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0});
        m_tsPrecip = new TimeSeriesUniform(Precipitation);
        m_tsPrecip->SetData(precip);
    }
    virtual void TearDown() {
        wxDELETE(m_tsPrecip);
    }
};

TEST_F(FluxWeightedModel, SingleUnitWith1Brick100Percent) {
    SettingsBasin basinProp;
    basinProp.AddHydroUnit(1, 100);
    basinProp.AddSurfaceElementToCurrentUnit("item-1", 1.0);
    basinProp.AddSurfaceElementToCurrentUnit("item-2", 0.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinProp));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model));
    EXPECT_TRUE(model.IsOk());

    EXPECT_TRUE(subBasin.AssignFractions(basinProp));

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge
    vecAxd basinOutputs = model.GetLogger()->GetAggregatedValues();

    vecDouble expectedOutputs = {0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0};

    for (int j = 0; j < basinOutputs[0].size(); ++j) {
        EXPECT_NEAR(basinOutputs[0][j], expectedOutputs[j], 0.000001);
    }

    // Check melt and swe
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();

    vecDouble expectedOutput1 = {0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0};
    vecDouble expectedOutput2 = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    vecDouble expectedOutput3 = {0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0};
    vecDouble expectedOutput4 = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    for (int j = 0; j < expectedOutput1.size(); ++j) {
        EXPECT_NEAR(unitContent[0](j, 0), expectedOutput1[j], 0.000001);
        EXPECT_NEAR(unitContent[1](j, 0), expectedOutput2[j], 0.000001);
        EXPECT_NEAR(unitContent[2](j, 0), expectedOutput3[j], 0.000001);
        EXPECT_NEAR(unitContent[3](j, 0), expectedOutput4[j], 0.000001);
    }
}

TEST_F(FluxWeightedModel, SingleUnitWith2Bricks50Percent) {
    SettingsBasin basinProp;
    basinProp.AddHydroUnit(1, 100);
    basinProp.AddSurfaceElementToCurrentUnit("item-1", 0.5);
    basinProp.AddSurfaceElementToCurrentUnit("item-2", 0.5);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinProp));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model));
    EXPECT_TRUE(model.IsOk());

    EXPECT_TRUE(subBasin.AssignFractions(basinProp));

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge
    vecAxd basinOutputs = model.GetLogger()->GetAggregatedValues();

    vecDouble expectedOutputs = {0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0};

    for (int j = 0; j < basinOutputs[0].size(); ++j) {
        EXPECT_NEAR(basinOutputs[0][j], expectedOutputs[j], 0.000001);
    }

    // Check melt and swe
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();

    vecDouble expectedOutput1 = {0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0};
    vecDouble expectedOutput2 = {0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0};
    vecDouble expectedOutput3 = {0.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 0.0, 0.0};
    vecDouble expectedOutput4 = {0.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 0.0, 0.0};

    for (int j = 0; j < expectedOutput1.size(); ++j) {
        EXPECT_NEAR(unitContent[0](j, 0), expectedOutput1[j], 0.000001);
        EXPECT_NEAR(unitContent[1](j, 0), expectedOutput2[j], 0.000001);
        EXPECT_NEAR(unitContent[2](j, 0), expectedOutput3[j], 0.000001);
        EXPECT_NEAR(unitContent[3](j, 0), expectedOutput4[j], 0.000001);
    }
}

TEST_F(FluxWeightedModel, SingleUnitWith2BricksDifferentPercent) {
    SettingsBasin basinProp;
    basinProp.AddHydroUnit(1, 100);
    basinProp.AddSurfaceElementToCurrentUnit("item-1", 0.7);
    basinProp.AddSurfaceElementToCurrentUnit("item-2", 0.3);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinProp));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model));
    EXPECT_TRUE(model.IsOk());

    EXPECT_TRUE(subBasin.AssignFractions(basinProp));

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge
    vecAxd basinOutputs = model.GetLogger()->GetAggregatedValues();

    vecDouble expectedOutputs = {0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0};

    for (int j = 0; j < basinOutputs[0].size(); ++j) {
        EXPECT_NEAR(basinOutputs[0][j], expectedOutputs[j], 0.000001);
    }

    // Check melt and swe
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();

    vecDouble expectedOutput1 = {0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0};
    vecDouble expectedOutput2 = {0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0};
    vecDouble expectedOutput3 = {0.0, 7.0, 7.0, 7.0, 7.0, 7.0, 7.0, 7.0, 0.0, 0.0};
    vecDouble expectedOutput4 = {0.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 0.0, 0.0};

    for (int j = 0; j < expectedOutput1.size(); ++j) {
        EXPECT_NEAR(unitContent[0](j, 0), expectedOutput1[j], 0.000001);
        EXPECT_NEAR(unitContent[1](j, 0), expectedOutput2[j], 0.000001);
        EXPECT_NEAR(unitContent[2](j, 0), expectedOutput3[j], 0.000001);
        EXPECT_NEAR(unitContent[3](j, 0), expectedOutput4[j], 0.000001);
    }
}
