#include <gtest/gtest.h>
#include <wx/stdpaths.h>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

class ModelSocontBasic : public ::testing::Test {
  protected:
    SettingsModel m_model;
    TimeSeriesUniform* m_tsPrecip{};
    TimeSeriesUniform* m_tsTemp{};
    TimeSeriesUniform* m_tsPet{};

    void SetUp() override {
        m_model.SetSolver("HeunExplicit");
        m_model.SetTimer("2020-01-01", "2020-01-10", 1, "Day");
        m_model.SetLogAll(true);

        vecStr landCoverTypes = {"ground", "glacier"};
        vecStr landCoverNames = {"ground", "glacier"};
        m_model.GenerateStructureSocont(landCoverTypes, landCoverNames, 2, "linear-storage");

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

TEST_F(ModelSocontBasic, ModelBuildsCorrectly) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);

    EXPECT_TRUE(model.Initialize(m_model));
    EXPECT_TRUE(model.IsOk());
}

TEST_F(ModelSocontBasic, ModelRunsCorrectly) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model));
    EXPECT_TRUE(model.IsOk());

    EXPECT_TRUE(subBasin.AssignFractions(basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPet));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
}

TEST_F(ModelSocontBasic, WaterBalanceCloses) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model));
    EXPECT_TRUE(model.IsOk());

    EXPECT_TRUE(subBasin.AssignFractions(basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPet));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    vecAxd basinContent = model.GetLogger()->GetSubBasinValues();
    // [0] "glacier-area-rain-snowmelt-storage:content"
    // [1] "glacier-area-rain-snowmelt-storage:outflow:output"
    // [2] "glacier-area-icemelt-storage:content"
    // [3] "glacier-area-icemelt-storage:outflow:output"
    // [4] "outlet"
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    // [0] "ground:content"
    // [1] "ground:infiltration:output"
    // [2] "ground:runoff:output"
    // [3] "glacier:content"
    // [4] "glacier:outflow-rain-snowmelt:output"
    // [5] "glacier:melt:output"
    // [6] "ground-snowpack:content"
    // [7] "ground-snowpack:snow"
    // [8] "ground-snowpack:melt:output"
    // [9] "glacier-snowpack:content"
    // [10] "glacier-snowpack:snow"
    // [11] "glacier-snowpack:melt:output"
    // [12] "slow-reservoir:content"
    // [13] "slow-reservoir:ET:output"
    // [14] "slow-reservoir:outflow:output"
    // [15] "slow-reservoir:percolation:output"
    // [16] "slow-reservoir:overflow:output"
    // [17] "slow-reservoir-2:content"
    // [18] "slow-reservoir-2:outflow:output"
    // [19] "surface-runoff:content"
    // [20] "surface-runoff:outflow:output"

    // Input
    double precip = 80;

    // Output
    double discharge = basinContent[4].sum();
    double et = unitContent[13].sum();

    // Last state values
    double sb0 = basinContent[0].tail(1)[0];
    double sb2 = basinContent[2].tail(1)[0];
    double su0 = unitContent[0](Eigen::last, Eigen::all).sum();
    double su3 = unitContent[3](Eigen::last, Eigen::all).sum();
    double su6 = unitContent[6](Eigen::last, Eigen::all).sum();
    double su7 = unitContent[7](Eigen::last, Eigen::all).sum();
    double su9 = unitContent[9](Eigen::last, Eigen::all).sum();
    double su10 = unitContent[10](Eigen::last, Eigen::all).sum();
    double su12 = unitContent[12](Eigen::last, Eigen::all).sum();
    double su17 = unitContent[17](Eigen::last, Eigen::all).sum();
    double su19 = unitContent[19](Eigen::last, Eigen::all).sum();
    double stateBasin = sb0 + sb2;
    double stateUnits = su0 + su3 + su6 + su7 + su9 + su10 + su12 + su17 + su19;

    double balance = precip - discharge - et - stateBasin - stateUnits;

    ASSERT_NEAR(balance, 0, 0.00002);
}