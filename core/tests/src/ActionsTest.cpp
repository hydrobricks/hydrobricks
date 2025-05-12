#include <gtest/gtest.h>

#include "ActionLandCoverChange.h"
#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"
#include "helpers.h"

class ActionsInModel : public ::testing::Test {
  protected:
    SettingsModel m_model;
    TimeSeriesUniform* m_tsPrecip{};
    TimeSeriesUniform* m_tsTemp{};
    TimeSeriesUniform* m_tsPet{};

    void SetUp() override {
        m_model.SetLogAll();
        m_model.SetSolver("heun_explicit");
        m_model.SetTimer("2020-01-01", "2020-01-10", 1, "day");
        m_model.SetLogAll(true);

        vecStr landCoverTypes = {"ground", "glacier"};
        vecStr landCoverNames = {"ground", "glacier"};
        GenerateStructureSocont(m_model, landCoverTypes, landCoverNames, 2, "linear_storage");

        auto precip = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0});
        m_tsPrecip = new TimeSeriesUniform(Precipitation);
        m_tsPrecip->SetData(precip);

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        temperature->SetValues({-2.0, -1.0, -1.0, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 9.0});
        m_tsTemp = new TimeSeriesUniform(Temperature);
        m_tsTemp->SetData(temperature);

        auto pet = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        pet->SetValues({1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0});
        m_tsPet = new TimeSeriesUniform(PET);
        m_tsPet->SetData(pet);
    }
    void TearDown() override {
        wxDELETE(m_tsPrecip);
        wxDELETE(m_tsTemp);
        wxDELETE(m_tsPet);
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
    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPet));
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

class ActionsInModel2LandCovers : public ::testing::Test {
  protected:
    SettingsModel m_model;
    TimeSeriesUniform* m_tsPrecip{};
    TimeSeriesUniform* m_tsTemp{};
    TimeSeriesUniform* m_tsPet{};

    void SetUp() override {
        m_model.SetLogAll();
        m_model.SetSolver("heun_explicit");
        m_model.SetTimer("2020-01-01", "2020-01-10", 1, "day");
        m_model.SetLogAll(true);

        vecStr landCoverTypes = {"ground", "glacier", "glacier"};
        vecStr landCoverNames = {"ground", "glacier_ice", "glacier_debris"};
        GenerateStructureSocont(m_model, landCoverTypes, landCoverNames, 2, "linear_storage");

        auto precip = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0});
        m_tsPrecip = new TimeSeriesUniform(Precipitation);
        m_tsPrecip->SetData(precip);

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        temperature->SetValues({-2.0, -1.0, -1.0, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 9.0});
        m_tsTemp = new TimeSeriesUniform(Temperature);
        m_tsTemp->SetData(temperature);

        auto pet = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        pet->SetValues({1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0});
        m_tsPet = new TimeSeriesUniform(PET);
        m_tsPet->SetData(pet);
    }
    void TearDown() override {
        wxDELETE(m_tsPrecip);
        wxDELETE(m_tsTemp);
        wxDELETE(m_tsPet);
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
    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPet));
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
    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPet));
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

    vecDouble dates = model.GetActionsManager()->GetDates();

    for (int i = 0; i < dates.size() - 1; ++i) {
        EXPECT_TRUE(dates[i] <= dates[i + 1]);
    }

    EXPECT_TRUE(model.Run());
}