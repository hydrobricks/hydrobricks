#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "SettingsBasin.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

/**
 * Model: simple snowpack model
 */

class FluxWeightedModel : public ::testing::Test {
  protected:
    SettingsModel m_model;
    TimeSeriesUniform* m_tsPrecip{};

    void SetUp() override {
        m_model.SetSolver("HeunExplicit");
        m_model.SetTimer("2020-01-01", "2020-01-10", 1, "Day");
        m_model.SetLogAll(true);

        // Precipitation
        m_model.GeneratePrecipitationSplitters(false);

        // Land cover elements and processes
        m_model.AddLandCoverBrick("item-1", "GenericLandCover");
        m_model.AddBrickProcess("outflow", "Outflow:direct", "outlet");
        m_model.AddLandCoverBrick("item-2", "GenericLandCover");
        m_model.AddBrickProcess("outflow", "Outflow:direct", "outlet");

        // Outlet
        m_model.AddLoggingToItem("outlet");

        auto precip = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0});
        m_tsPrecip = new TimeSeriesUniform(Precipitation);
        m_tsPrecip->SetData(precip);
    }
    void TearDown() override {
        wxDELETE(m_tsPrecip);
    }
};

TEST_F(FluxWeightedModel, SingleUnitWith1Brick100Percent) {
    SettingsBasin basinProp;
    basinProp.AddHydroUnit(1, 100);
    basinProp.AddLandCover("item-1", "", 1.0);
    basinProp.AddLandCover("item-2", "", 0.0);

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
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble expectedOutputs = {0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0};

    for (int j = 0; j < basinOutputs[0].size(); ++j) {
        EXPECT_NEAR(basinOutputs[0][j], expectedOutputs[j], 0.000001);
    }

    // Check melt and swe
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();

    vecDouble expectedOutput1 = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    vecDouble expectedOutput2 = {0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0};
    vecDouble expectedOutput3 = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
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
    basinProp.AddLandCover("item-1", "", 0.5);
    basinProp.AddLandCover("item-2", "", 0.5);

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
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble expectedOutputs = {0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0};

    for (int j = 0; j < basinOutputs[0].size(); ++j) {
        EXPECT_NEAR(basinOutputs[0][j], expectedOutputs[j], 0.000001);
    }

    // Check hydro unit values
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    // [0] "item-1:content"
    // [1] "item-1:outflow:output"
    // [2] "item-2:content"
    // [3] "item-2:outflow:output"

    vecDouble expectedOutput1 = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    vecDouble expectedOutput2 = {0.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 0.0, 0.0};
    vecDouble expectedOutput3 = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
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
    basinProp.AddLandCover("item-1", "", 0.7);
    basinProp.AddLandCover("item-2", "", 0.3);

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
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble expectedOutputs = {0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0};

    for (int j = 0; j < basinOutputs[0].size(); ++j) {
        EXPECT_NEAR(basinOutputs[0][j], expectedOutputs[j], 0.000001);
    }

    // Check hydro unit values
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();

    vecDouble expectedOutput1 = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    vecDouble expectedOutput2 = {0.0, 7.0, 7.0, 7.0, 7.0, 7.0, 7.0, 7.0, 0.0, 0.0};
    vecDouble expectedOutput3 = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    vecDouble expectedOutput4 = {0.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 0.0, 0.0};

    for (int j = 0; j < expectedOutput1.size(); ++j) {
        EXPECT_NEAR(unitContent[0](j, 0), expectedOutput1[j], 0.000001);
        EXPECT_NEAR(unitContent[1](j, 0), expectedOutput2[j], 0.000001);
        EXPECT_NEAR(unitContent[2](j, 0), expectedOutput3[j], 0.000001);
        EXPECT_NEAR(unitContent[3](j, 0), expectedOutput4[j], 0.000001);
    }
}

TEST_F(FluxWeightedModel, TwoUnitsWithTwoLandCoverBricks) {
    SettingsBasin basinProp;
    basinProp.AddHydroUnit(1, 150);
    basinProp.AddLandCover("item-1", "", 0.5);
    basinProp.AddLandCover("item-2", "", 0.5);
    basinProp.AddHydroUnit(1, 50);
    basinProp.AddLandCover("item-1", "", 0.5);
    basinProp.AddLandCover("item-2", "", 0.5);

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
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble expectedOutputs = {0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0};

    for (int j = 0; j < basinOutputs[0].size(); ++j) {
        EXPECT_NEAR(basinOutputs[0][j], expectedOutputs[j], 0.000001);
    }

    // Check hydro unit values
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    // [0] "item-1:content"
    // [1] "item-1:outflow:output"
    // [2] "item-2:content"
    // [3] "item-2:outflow:output"

    vecDouble expectedOutput1 = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    vecDouble expectedOutput2 = {0.0, 3.75, 3.75, 3.75, 3.75, 3.75, 3.75, 3.75, 0.0, 0.0};
    vecDouble expectedOutput3 = {0.0, 1.25, 1.25, 1.25, 1.25, 1.25, 1.25, 1.25, 0.0, 0.0};

    for (int j = 0; j < expectedOutput1.size(); ++j) {
        EXPECT_NEAR(unitContent[0](j, 0), expectedOutput1[j], 0.000001);
        EXPECT_NEAR(unitContent[1](j, 0), expectedOutput2[j], 0.000001);
        EXPECT_NEAR(unitContent[2](j, 0), expectedOutput1[j], 0.000001);
        EXPECT_NEAR(unitContent[3](j, 0), expectedOutput2[j], 0.000001);

        EXPECT_NEAR(unitContent[0](j, 1), expectedOutput1[j], 0.000001);
        EXPECT_NEAR(unitContent[1](j, 1), expectedOutput3[j], 0.000001);
        EXPECT_NEAR(unitContent[2](j, 1), expectedOutput1[j], 0.000001);
        EXPECT_NEAR(unitContent[3](j, 1), expectedOutput3[j], 0.000001);
    }
}

TEST_F(FluxWeightedModel, TwoUnitsWithTwoLandCoverBricksDifferentArea) {
    SettingsBasin basinProp;
    basinProp.AddHydroUnit(1, 150);
    basinProp.AddLandCover("item-1", "", 2.0 / 3.0);
    basinProp.AddLandCover("item-2", "", 1.0 / 3.0);
    basinProp.AddHydroUnit(1, 50);
    basinProp.AddLandCover("item-1", "", 4.0 / 5.0);
    basinProp.AddLandCover("item-2", "", 1.0 / 5.0);

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
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble expectedOutputs = {0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0};

    for (int j = 0; j < basinOutputs[0].size(); ++j) {
        EXPECT_NEAR(basinOutputs[0][j], expectedOutputs[j], 0.000001);
    }

    // Check hydro unit values
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();

    vecDouble expectedOutput1 = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    vecDouble expectedOutput2 = {0.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 0.0, 0.0};
    vecDouble expectedOutput3 = {0.0, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 0.0, 0.0};
    vecDouble expectedOutput4 = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    vecDouble expectedOutput5 = {0.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 0.0};
    vecDouble expectedOutput6 = {0.0, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.0, 0.0};

    for (int j = 0; j < expectedOutput1.size(); ++j) {
        EXPECT_NEAR(unitContent[0](j, 0), expectedOutput1[j], 0.000001);
        EXPECT_NEAR(unitContent[1](j, 0), expectedOutput2[j], 0.000001);
        EXPECT_NEAR(unitContent[2](j, 0), expectedOutput4[j], 0.000001);
        EXPECT_NEAR(unitContent[3](j, 0), expectedOutput3[j], 0.000001);

        EXPECT_NEAR(unitContent[0](j, 1), expectedOutput1[j], 0.000001);
        EXPECT_NEAR(unitContent[1](j, 1), expectedOutput5[j], 0.000001);
        EXPECT_NEAR(unitContent[2](j, 1), expectedOutput4[j], 0.000001);
        EXPECT_NEAR(unitContent[3](j, 1), expectedOutput6[j], 0.000001);
    }
}
