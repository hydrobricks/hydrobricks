#include <gtest/gtest.h>

#include <memory>

#include "ActionGlacierEvolutionDeltaH.h"
#include "ActionGlacierSnowToIceTransformation.h"
#include "ActionLandCoverChange.h"
#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"
#include "helpers.h"

class ActionsInModel : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;
    std::unique_ptr<TimeSeriesUniform> _tsTemp;
    std::unique_ptr<TimeSeriesUniform> _tsPet;

    void SetUp() override {
        _model.SetLogAll();
        _model.SetSolver("heun_explicit");
        _model.SetTimer("2020-01-01", "2020-01-10", 1, "day");
        _model.SetLogAll(true);

        vecStr landCoverTypes = {"ground", "glacier"};
        vecStr landCoverNames = {"ground", "glacier"};
        GenerateStructureSocont(_model, landCoverTypes, landCoverNames, 2, "linear_storage", false);

        auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(Precipitation);
        _tsPrecip->SetData(std::move(precip));

        auto temperature = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        temperature->SetValues({-2.0, -1.0, -1.0, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 9.0});
        _tsTemp = std::make_unique<TimeSeriesUniform>(Temperature);
        _tsTemp->SetData(std::move(temperature));

        auto pet = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        pet->SetValues({1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0});
        _tsPet = std::make_unique<TimeSeriesUniform>(PET);
        _tsPet->SetData(std::move(pet));
    }
    void TearDown() override {
        // Automatic cleanup via unique_ptr - no manual deletion needed
    }
};

TEST_F(ActionsInModel, LandCoverChangeWorks) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);
    basinSettings.AddHydroUnit(2, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);
    basinSettings.AddHydroUnit(3, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    ActionLandCoverChange action;
    action.AddChange(GetMJD(2020, 1, 2), 2, "glacier", 60);
    action.AddChange(GetMJD(2020, 1, 4), 2, "glacier", 70);
    action.AddChange(GetMJD(2020, 1, 6), 2, "glacier", 80);
    action.AddChange(GetMJD(2020, 1, 6), 3, "glacier", 40);
    EXPECT_TRUE(model.AddAction(&action));

    EXPECT_EQ(model.GetActionCount(), 1);
    EXPECT_EQ(model.GetSporadicActionItemCount(), 4);

    EXPECT_TRUE(model.Run());

    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(0)->GetLandCover("glacier")->GetAreaFraction(), 0.5f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(1)->GetLandCover("glacier")->GetAreaFraction(), 0.8f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(1)->GetLandCover("ground")->GetAreaFraction(), 0.2f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(2)->GetLandCover("glacier")->GetAreaFraction(), 0.4f);
}

TEST_F(ActionsInModel, GlacierEvolutionDeltaHWorks) {
    // Change the model settings to cover a larger time period
    _model.SetTimer("2000-01-01", "2020-12-31", 1, "day");
    int timeStepCount = GetMJD(2020, 12, 31) - GetMJD(2000, 1, 1) + 1;

    auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2000, 1, 1), GetMJD(2020, 12, 31), 1, Day);
    vecDouble valuesPrecip(timeStepCount, 0.0);
    precip->SetValues(valuesPrecip);
    _tsPrecip->SetData(std::move(precip));

    auto temperature = std::make_unique<TimeSeriesDataRegular>(GetMJD(2000, 1, 1), GetMJD(2020, 12, 31), 1, Day);
    vecDouble valuesTemp(timeStepCount, 2.0);
    temperature->SetValues(valuesTemp);
    _tsTemp->SetData(std::move(temperature));

    auto pet = std::make_unique<TimeSeriesDataRegular>(GetMJD(2000, 1, 1), GetMJD(2020, 12, 31), 1, Day);
    vecDouble valuesPet(timeStepCount, 0.0);
    pet->SetValues(valuesPet);
    _tsPet->SetData(std::move(pet));

    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 10000);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);
    basinSettings.AddHydroUnit(2, 10000);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);
    basinSettings.AddHydroUnit(3, 10000);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    ActionGlacierEvolutionDeltaH action;
    axi hydroUnitIds(3);
    hydroUnitIds << 1, 2, 3;
    axxd areas = axxd::Zero(4, 3);
    areas << 5000, 5000, 5000, 2000, 3000, 4000, 0, 1000, 2000, 0, 0, 0;
    axxd volumes = axxd::Zero(4, 3);
    volumes << 50000, 50000, 50000, 10000, 35000, 40000, 0, 5000, 10000, 0, 0, 0;
    action.AddLookupTables(10, "glacier", hydroUnitIds, areas, volumes);

    EXPECT_TRUE(model.AddAction(&action));

    EXPECT_EQ(model.GetActionCount(), 1);

    EXPECT_TRUE(model.Run());

    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(0)->GetLandCover("glacier")->GetAreaFraction(), 0.0f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(1)->GetLandCover("glacier")->GetAreaFraction(), 0.0f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(1)->GetLandCover("ground")->GetAreaFraction(), 1.0f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(2)->GetLandCover("glacier")->GetAreaFraction(), 0.0f);
}

class ActionsInModel2LandCovers : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;
    std::unique_ptr<TimeSeriesUniform> _tsTemp;
    std::unique_ptr<TimeSeriesUniform> _tsPet;

    void SetUp() override {
        _model.SetLogAll();
        _model.SetSolver("heun_explicit");
        _model.SetTimer("2020-01-01", "2020-01-10", 1, "day");
        _model.SetLogAll(true);

        vecStr landCoverTypes = {"ground", "glacier", "glacier"};
        vecStr landCoverNames = {"ground", "glacier_ice", "glacier_debris"};
        GenerateStructureSocont(_model, landCoverTypes, landCoverNames, 2, "linear_storage");

        auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(Precipitation);
        _tsPrecip->SetData(std::move(precip));

        auto temperature = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        temperature->SetValues({-2.0, -1.0, -1.0, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 9.0});
        _tsTemp = std::make_unique<TimeSeriesUniform>(Temperature);
        _tsTemp->SetData(std::move(temperature));

        auto pet = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        pet->SetValues({1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0});
        _tsPet = std::make_unique<TimeSeriesUniform>(PET);
        _tsPet->SetData(std::move(pet));
    }
    void TearDown() override {
        // Automatic cleanup via unique_ptr - no manual deletion needed
    }
};

TEST_F(ActionsInModel2LandCovers, LandCoverChangeWorks) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier_ice", "", 0.0);
    basinSettings.AddLandCover("glacier_debris", "", 0.5);
    basinSettings.AddHydroUnit(2, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier_ice", "", 0.3);
    basinSettings.AddLandCover("glacier_debris", "", 0.2);
    basinSettings.AddHydroUnit(3, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier_ice", "", 0.3);
    basinSettings.AddLandCover("glacier_debris", "", 0.2);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    ActionLandCoverChange action;
    action.AddChange(GetMJD(2020, 1, 2), 2, "glacier_ice", 20);
    action.AddChange(GetMJD(2020, 1, 2), 2, "glacier_debris", 30);
    action.AddChange(GetMJD(2020, 1, 4), 2, "glacier_ice", 10);
    action.AddChange(GetMJD(2020, 1, 4), 2, "glacier_debris", 40);
    action.AddChange(GetMJD(2020, 1, 6), 3, "glacier_ice", 20);
    action.AddChange(GetMJD(2020, 1, 6), 3, "glacier_debris", 30);
    // Note: changing the ground area will impact the balance here.

    EXPECT_TRUE(model.AddAction(&action));

    EXPECT_TRUE(model.Run());

    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(0)->GetLandCover("ground")->GetAreaFraction(), 0.5f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(0)->GetLandCover("glacier_ice")->GetAreaFraction(), 0.0f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(0)->GetLandCover("glacier_debris")->GetAreaFraction(), 0.5f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(1)->GetLandCover("ground")->GetAreaFraction(), 0.5f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(1)->GetLandCover("glacier_ice")->GetAreaFraction(), 0.1f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(1)->GetLandCover("glacier_debris")->GetAreaFraction(), 0.4f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(2)->GetLandCover("ground")->GetAreaFraction(), 0.5f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(2)->GetLandCover("glacier_ice")->GetAreaFraction(), 0.2f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(2)->GetLandCover("glacier_debris")->GetAreaFraction(), 0.3f);

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 80;
    double totalGlacierMelt = logger->GetTotalHydroUnits("glacier_ice:melt:output");
    totalGlacierMelt += logger->GetTotalHydroUnits("glacier_debris:melt:output");
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip - totalGlacierMelt;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ActionsInModel2LandCovers, DatesGetSortedCorrectly) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier_ice", "", 0.3);
    basinSettings.AddLandCover("glacier_debris", "", 0.2);
    basinSettings.AddHydroUnit(2, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier_ice", "", 0.3);
    basinSettings.AddLandCover("glacier_debris", "", 0.2);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    ActionLandCoverChange action;
    action.AddChange(GetMJD(2020, 1, 2), 1, "glacier_ice", 20);
    action.AddChange(GetMJD(2020, 1, 4), 1, "glacier_ice", 10);
    action.AddChange(GetMJD(2020, 1, 6), 2, "glacier_ice", 20);
    action.AddChange(GetMJD(2020, 1, 2), 1, "glacier_debris", 30);
    action.AddChange(GetMJD(2020, 1, 4), 1, "glacier_debris", 40);
    action.AddChange(GetMJD(2020, 1, 6), 2, "glacier_debris", 30);
    // Note: changing the ground area will impact the balance here.

    EXPECT_TRUE(model.AddAction(&action));

    vecDouble dates = model.GetActionsManager()->GetSporadicActionDates();

    for (int i = 0; i < dates.size() - 1; ++i) {
        EXPECT_TRUE(dates[i] <= dates[i + 1]);
    }

    EXPECT_TRUE(model.Run());
}

TEST_F(ActionsInModel, GlacierSnowToIceTransformationWorks) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);

    _model.SetTimer("2020-09-25", "2020-10-04", 1, "day");

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 9, 25), GetMJD(2020, 10, 4), 1, Day);
    precip->SetValues({0.0, 50.0, 50.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    auto tsPrecip = std::make_unique<TimeSeriesUniform>(Precipitation);
    tsPrecip->SetData(std::move(precip));

    auto temperature = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 9, 25), GetMJD(2020, 10, 4), 1, Day);
    temperature->SetValues({-5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0});
    auto tsTemp = std::make_unique<TimeSeriesUniform>(Temperature);
    tsTemp->SetData(std::move(temperature));

    auto pet = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 9, 25), GetMJD(2020, 10, 4), 1, Day);
    pet->SetValues({0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    auto tsPet = std::make_unique<TimeSeriesUniform>(PET);
    tsPet->SetData(std::move(pet));

    subBasin.GetHydroUnit(0)->GetLandCover("glacier")->UpdateContent(10000.0f, ContentType::Ice);

    ASSERT_TRUE(model.AddTimeSeries(std::move(tsPrecip)));
    ASSERT_TRUE(model.AddTimeSeries(std::move(tsTemp)));
    ASSERT_TRUE(model.AddTimeSeries(std::move(tsPet)));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    ActionGlacierSnowToIceTransformation action(9, 30, "glacier");
    EXPECT_TRUE(model.AddAction(&action));

    EXPECT_EQ(model.GetActionCount(), 1);

    EXPECT_TRUE(model.Run());

    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(0)->GetBrick("glacier_snowpack")->GetContent(ContentType::Snow), 0.0f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(0)->GetLandCover("glacier")->GetContent(ContentType::Ice), 10100.0f);
}
