#include <gtest/gtest.h>

#include "BehaviourLandCoverChange.h"
#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

class BehavioursInModel : public ::testing::Test {
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
        m_model.GenerateStructureSocont(landCoverTypes, landCoverNames, 2, "linear_storage");

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

TEST_F(BehavioursInModel, LandCoverChangeWorks) {
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

    BehaviourLandCoverChange behaviour;
    behaviour.AddChange(GetMJD(2020, 1, 2), 2, "glacier", 60);
    behaviour.AddChange(GetMJD(2020, 1, 4), 2, "glacier", 70);
    behaviour.AddChange(GetMJD(2020, 1, 6), 2, "glacier", 80);
    behaviour.AddChange(GetMJD(2020, 1, 6), 3, "glacier", 40);
    EXPECT_TRUE(model.AddBehaviour(&behaviour));

    EXPECT_EQ(model.GetBehavioursNb(), 1);
    EXPECT_EQ(model.GetBehaviourItemsNb(), 4);

    EXPECT_TRUE(model.Run());

    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(0)->GetLandCover("glacier")->GetAreaFraction(), 0.5f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(1)->GetLandCover("glacier")->GetAreaFraction(), 0.8f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(1)->GetLandCover("ground")->GetAreaFraction(), 0.2f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(2)->GetLandCover("glacier")->GetAreaFraction(), 0.4f);
}

class BehavioursInModel2LandCovers : public ::testing::Test {
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
        vecStr landCoverNames = {"ground", "glacier-ice", "glacier-debris"};
        m_model.GenerateStructureSocont(landCoverTypes, landCoverNames, 2, "linear_storage");

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

TEST_F(BehavioursInModel2LandCovers, LandCoverChangeWorks) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier-ice", "", 0.3);
    basinSettings.AddLandCover("glacier-debris", "", 0.2);
    basinSettings.AddHydroUnit(2, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier-ice", "", 0.3);
    basinSettings.AddLandCover("glacier-debris", "", 0.2);
    basinSettings.AddHydroUnit(3, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier-ice", "", 0.3);
    basinSettings.AddLandCover("glacier-debris", "", 0.2);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPet));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    BehaviourLandCoverChange behaviour;
    behaviour.AddChange(GetMJD(2020, 1, 2), 2, "glacier-ice", 20);
    behaviour.AddChange(GetMJD(2020, 1, 2), 2, "glacier-debris", 30);
    behaviour.AddChange(GetMJD(2020, 1, 4), 2, "glacier-ice", 10);
    behaviour.AddChange(GetMJD(2020, 1, 4), 2, "glacier-debris", 40);
    behaviour.AddChange(GetMJD(2020, 1, 6), 2, "glacier-ice", 0);
    behaviour.AddChange(GetMJD(2020, 1, 6), 3, "glacier-ice", 20);
    behaviour.AddChange(GetMJD(2020, 1, 6), 3, "glacier-debris", 10);
    EXPECT_TRUE(model.AddBehaviour(&behaviour));

    EXPECT_TRUE(model.Run());

    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(0)->GetLandCover("ground")->GetAreaFraction(), 0.5f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(0)->GetLandCover("glacier-ice")->GetAreaFraction(), 0.3f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(0)->GetLandCover("glacier-debris")->GetAreaFraction(), 0.2f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(1)->GetLandCover("ground")->GetAreaFraction(), 0.6f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(1)->GetLandCover("glacier-ice")->GetAreaFraction(), 0.0f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(1)->GetLandCover("glacier-debris")->GetAreaFraction(), 0.4f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(2)->GetLandCover("ground")->GetAreaFraction(), 0.7f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(2)->GetLandCover("glacier-ice")->GetAreaFraction(), 0.2f);
    EXPECT_FLOAT_EQ(subBasin.GetHydroUnit(2)->GetLandCover("glacier-debris")->GetAreaFraction(), 0.1f);
}