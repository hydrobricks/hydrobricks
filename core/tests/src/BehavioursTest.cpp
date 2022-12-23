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
    behaviour.AddChange(GetMJD(2020, 1, 5), 2, "glacier", 80);

    EXPECT_TRUE(model.Run());

    EXPECT_EQ(model.GetSubBasin()->GetHydroUnit(0)->GetLandCover("glacier")->GetAreaFraction(), 0);
    EXPECT_EQ(model.GetSubBasin()->GetHydroUnit(1)->GetLandCover("glacier")->GetAreaFraction(), 0);
    EXPECT_EQ(model.GetSubBasin()->GetHydroUnit(2)->GetLandCover("glacier")->GetAreaFraction(), 0);
}
