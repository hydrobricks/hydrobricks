#include <gtest/gtest.h>

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
    TimeSeriesUniform* _tsPrecip{};
    TimeSeriesUniform* _tsTemp{};
    TimeSeriesUniform* _tsPet{};

    void SetUp() override {
        _model.SetLogAll();
        _model.SetSolver("heun_explicit");
        _model.SetTimer("2020-01-01", "2020-01-10", 1, "day");
        _model.SetLogAll(true);

        vecStr landCoverTypes = {"ground", "glacier"};
        vecStr landCoverNames = {"ground", "glacier"};
        GenerateStructureSocont(_model, landCoverTypes, landCoverNames, 2, "linear_storage", false);

        auto precip = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0});
        _tsPrecip = new TimeSeriesUniform(Precipitation);
        _tsPrecip->SetData(precip);

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        temperature->SetValues({-2.0, -1.0, -1.0, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 9.0});
        _tsTemp = new TimeSeriesUniform(Temperature);
        _tsTemp->SetData(temperature);

        auto pet = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        pet->SetValues({1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0});
        _tsPet = new TimeSeriesUniform(PET);
        _tsPet->SetData(pet);
    }
    void TearDown() override {
        wxDELETE(_tsPrecip);
        wxDELETE(_tsTemp);
        wxDELETE(_tsPet);
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
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(_tsPet));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    ActionLandCoverChange action;
    action.AddChange(GetMJD(2020, 1, 2), 2, "glacier", 60);
    action.AddChange(GetMJD(2020, 1, 4), 2, "glacier", 70);
    action.AddChange(GetMJD(2020, 1, 6), 2, "glacier", 80);
    action.AddChange(GetMJD(2020, 1, 6), 3, "glacier", 40);
    EXPECT_TRUE(model.AddAction(&action));

    EXPECT_EQ(model.GetActionsNb(), 1);
    EXPECT_EQ(model.GetSporadicActionItemsNb(), 4);

    EXPECT_TRUE(model.Run());

    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(0)->GetLandCover("glacier")->GetAreaFraction(), 0.5f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(1)->GetLandCover("glacier")->GetAreaFraction(), 0.8f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(1)->GetLandCover("ground")->GetAreaFraction(), 0.2f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(2)->GetLandCover("glacier")->GetAreaFraction(), 0.4f);
}

TEST_F(ActionsInModel, GlacierEvolutionDeltaHWorks) {
    // Change the model settings to cover a larger time period
    _model.SetTimer("2000-01-01", "2020-12-31", 1, "day");
    int nbTimeSteps = GetMJD(2020, 12, 31) - GetMJD(2000, 1, 1) + 1;

    auto precip = new TimeSeriesDataRegular(GetMJD(2000, 1, 1), GetMJD(2020, 12, 31), 1, Day);
    vecDouble valuesPrecip(nbTimeSteps, 0.0);
    precip->SetValues(valuesPrecip);
    _tsPrecip->SetData(precip);

    auto temperature = new TimeSeriesDataRegular(GetMJD(2000, 1, 1), GetMJD(2020, 12, 31), 1, Day);
    vecDouble valuesTemp(nbTimeSteps, 2.0);
    temperature->SetValues(valuesTemp);
    _tsTemp->SetData(temperature);

    auto pet = new TimeSeriesDataRegular(GetMJD(2000, 1, 1), GetMJD(2020, 12, 31), 1, Day);
    vecDouble valuesPet(nbTimeSteps, 0.0);
    pet->SetValues(valuesPet);
    _tsPet->SetData(pet);

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
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(_tsPet));
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

    EXPECT_EQ(model.GetActionsNb(), 1);

    EXPECT_TRUE(model.Run());

    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(0)->GetLandCover("glacier")->GetAreaFraction(), 0.0f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(1)->GetLandCover("glacier")->GetAreaFraction(), 0.0f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(1)->GetLandCover("ground")->GetAreaFraction(), 1.0f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(2)->GetLandCover("glacier")->GetAreaFraction(), 0.0f);
}

class ActionsInModel2LandCovers : public ::testing::Test {
  protected:
    SettingsModel _model;
    TimeSeriesUniform* _tsPrecip{};
    TimeSeriesUniform* _tsTemp{};
    TimeSeriesUniform* _tsPet{};

    void SetUp() override {
        _model.SetLogAll();
        _model.SetSolver("heun_explicit");
        _model.SetTimer("2020-01-01", "2020-01-10", 1, "day");
        _model.SetLogAll(true);

        vecStr landCoverTypes = {"ground", "glacier", "glacier"};
        vecStr landCoverNames = {"ground", "glacier_ice", "glacier_debris"};
        GenerateStructureSocont(_model, landCoverTypes, landCoverNames, 2, "linear_storage");

        auto precip = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0});
        _tsPrecip = new TimeSeriesUniform(Precipitation);
        _tsPrecip->SetData(precip);

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        temperature->SetValues({-2.0, -1.0, -1.0, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 9.0});
        _tsTemp = new TimeSeriesUniform(Temperature);
        _tsTemp->SetData(temperature);

        auto pet = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        pet->SetValues({1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0});
        _tsPet = new TimeSeriesUniform(PET);
        _tsPet->SetData(pet);
    }
    void TearDown() override {
        wxDELETE(_tsPrecip);
        wxDELETE(_tsTemp);
        wxDELETE(_tsPet);
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
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(_tsPet));
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
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(_tsPet));
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
    EXPECT_TRUE(model.IsOk());

    auto precip = new TimeSeriesDataRegular(GetMJD(2020, 9, 25), GetMJD(2020, 10, 4), 1, Day);
    precip->SetValues({0.0, 50.0, 50.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    auto tsPrecip = new TimeSeriesUniform(Precipitation);
    tsPrecip->SetData(precip);

    auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 9, 25), GetMJD(2020, 10, 4), 1, Day);
    temperature->SetValues({-5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0});
    auto tsTemp = new TimeSeriesUniform(Temperature);
    tsTemp->SetData(temperature);

    auto pet = new TimeSeriesDataRegular(GetMJD(2020, 9, 25), GetMJD(2020, 10, 4), 1, Day);
    pet->SetValues({0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    auto tsPet = new TimeSeriesUniform(PET);
    tsPet->SetData(pet);

    subBasin.GetHydroUnit(0)->GetLandCover("glacier")->UpdateContent(10000.0f, "ice");

    ASSERT_TRUE(model.AddTimeSeries(tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(tsPet));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    ActionGlacierSnowToIceTransformation action(9, 30, "glacier");
    EXPECT_TRUE(model.AddAction(&action));

    EXPECT_EQ(model.GetActionsNb(), 1);

    EXPECT_TRUE(model.Run());

    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(0)->GetBrick("glacier_snowpack")->GetContent("snow"), 0.0f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(0)->GetLandCover("glacier")->GetContent("ice"), 10100.0f);
}