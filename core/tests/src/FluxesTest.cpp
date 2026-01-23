#include <gtest/gtest.h>

#include <memory>

#include "ModelHydro.h"
#include "SettingsBasin.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

/**
 * Model: simple snowpack model
 */

class FluxWeightedModel : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;

    void SetUp() override {
        _model.SetSolver("heun_explicit");
        _model.SetTimer("2020-01-01", "2020-01-10", 1, "day");
        _model.SetLogAll(true);

        // Precipitation
        _model.GeneratePrecipitationSplitters(false);

        // Land cover elements and processes
        _model.AddLandCoverBrick("item_1", "generic_land_cover");
        _model.AddBrickProcess("outflow", "outflow:direct", "outlet");
        _model.AddLandCoverBrick("item_2", "generic_land_cover");
        _model.AddBrickProcess("outflow", "outflow:direct", "outlet");

        // Outlet
        _model.AddLoggingToItem("outlet");

        auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(Precipitation);
        _tsPrecip->SetData(std::move(precip));
    }
    void TearDown() override {
        // Automatic cleanup via unique_ptr
    }
};

TEST_F(FluxWeightedModel, SingleUnitWith1Brick100Percent) {
    SettingsBasin basinProp;
    basinProp.AddHydroUnit(1, 100);
    basinProp.AddLandCover("item_1", "", 1.0);
    basinProp.AddLandCover("item_2", "", 0.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinProp));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinProp));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
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

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 70;
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(FluxWeightedModel, SingleUnitWith2Bricks50Percent) {
    SettingsBasin basinProp;
    basinProp.AddHydroUnit(1, 100);
    basinProp.AddLandCover("item_1", "", 0.5);
    basinProp.AddLandCover("item_2", "", 0.5);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinProp));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinProp));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
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
    // [0] "item_1:content"
    // [1] "item_1:outflow:output"
    // [2] "item_2:content"
    // [3] "item_2:outflow:output"

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

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 70;
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(FluxWeightedModel, SingleUnitWith2BricksDifferentPercent) {
    SettingsBasin basinProp;
    basinProp.AddHydroUnit(1, 100);
    basinProp.AddLandCover("item_1", "", 0.7);
    basinProp.AddLandCover("item_2", "", 0.3);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinProp));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinProp));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
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

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 70;
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(FluxWeightedModel, TwoUnitsWithTwoLandCoverBricks) {
    SettingsBasin basinProp;
    basinProp.AddHydroUnit(1, 150);
    basinProp.AddLandCover("item_1", "", 0.5);
    basinProp.AddLandCover("item_2", "", 0.5);
    basinProp.AddHydroUnit(1, 50);
    basinProp.AddLandCover("item_1", "", 0.5);
    basinProp.AddLandCover("item_2", "", 0.5);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinProp));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinProp));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
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
    // [0] "item_1:content"
    // [1] "item_1:outflow:output"
    // [2] "item_2:content"
    // [3] "item_2:outflow:output"

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

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 70;
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(FluxWeightedModel, TwoUnitsWithTwoLandCoverBricksDifferentArea) {
    SettingsBasin basinProp;
    basinProp.AddHydroUnit(1, 150);
    basinProp.AddLandCover("item_1", "", 2.0 / 3.0);
    basinProp.AddLandCover("item_2", "", 1.0 / 3.0);
    basinProp.AddHydroUnit(1, 50);
    basinProp.AddLandCover("item_1", "", 4.0 / 5.0);
    basinProp.AddLandCover("item_2", "", 1.0 / 5.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinProp));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinProp));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
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

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 70;
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST(Fluxes, ToAtmosphereInstantaneous) {
    SettingsModel model;
    model.SetSolver("heun_explicit");
    model.SetTimer("2020-01-01", "2020-01-10", 1, "day");

    // Minimal structure with a land cover brick
    model.AddLandCoverBrick("ground", "generic_land_cover");
    model.SelectHydroUnitBrick("ground");
    model.AddBrickProcess("outflow", "outflow:direct", "outlet");

    auto petData = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
    petData->SetValues({1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0});

    auto tsPet = std::make_unique<TimeSeriesUniform>(PET);
    tsPet->SetData(std::move(petData));

    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro hydroModel(&subBasin);
    EXPECT_TRUE(hydroModel.Initialize(model, basinSettings));

    ASSERT_TRUE(hydroModel.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsPet))));
    ASSERT_TRUE(hydroModel.AttachTimeSeriesToHydroUnits());

    // Run and check that fluxes are computed without errors
    EXPECT_TRUE(hydroModel.Run());
}
