#include <gtest/gtest.h>
#include <wx/stdpaths.h>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"
#include "helpers.h"

class ModelSocontBasic : public ::testing::Test {
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

TEST_F(ModelSocontBasic, ModelBuildsCorrectly) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);

    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
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
    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPet));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
}

TEST_F(ModelSocontBasic, WaterBalanceClosesWithoutGlacierMelt) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    m_model.SelectHydroUnitBrick("glacier");
    m_model.SelectProcess("melt");
    m_model.SetProcessParameterValue("degree_day_factor", 0.0f);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPet));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 80;
    double totalGlacierMelt = logger->GetTotalHydroUnits("glacier:melt:output");
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip - totalGlacierMelt;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelSocontBasic, WaterBalanceClosesOnlyIceMelt) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    auto precipValues = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
    precipValues->SetValues({0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    auto* tsPrecip = new TimeSeriesUniform(Precipitation);
    tsPrecip->SetData(precipValues);

    ASSERT_TRUE(model.AddTimeSeries(tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPet));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 0;
    double totalGlacierMelt = logger->GetTotalHydroUnits("glacier:melt:output");
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip - totalGlacierMelt;

    EXPECT_NEAR(totalGlacierMelt, 48.0, 0.0000001);
    EXPECT_NEAR(balance, 0.0, 0.0000001);

    wxDELETE(tsPrecip);
}

TEST_F(ModelSocontBasic, WaterBalanceClosesWithHeunExplicit) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
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

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 80;
    double totalGlacierMelt = logger->GetTotalHydroUnits("glacier:melt:output");
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip - totalGlacierMelt;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelSocontBasic, WaterBalanceClosesWithEulerExplicit) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    m_model.SetSolver("euler_explicit");
    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPet));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 80;
    double totalGlacierMelt = logger->GetTotalHydroUnits("glacier:melt:output");
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip - totalGlacierMelt;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelSocontBasic, WaterBalanceClosesWithRungeKutta) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    m_model.SetSolver("runge_kutta");
    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPet));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 80;
    double totalGlacierMelt = logger->GetTotalHydroUnits("glacier:melt:output");
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip - totalGlacierMelt;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelSocontBasic, WaterBalanceClosesWith2HydroUnitsIceMeltOnly) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);
    basinSettings.AddHydroUnit(2, 50);
    basinSettings.AddLandCover("ground", "", 0.2);
    basinSettings.AddLandCover("glacier", "", 0.8);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    auto precipValues = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
    precipValues->SetValues({0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    auto* tsPrecip = new TimeSeriesUniform(Precipitation);
    tsPrecip->SetData(precipValues);

    ASSERT_TRUE(model.AddTimeSeries(tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPet));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 0;
    double totalGlacierMelt = logger->GetTotalHydroUnits("glacier:melt:output");
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip - totalGlacierMelt;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelSocontBasic, WaterBalanceClosesWith2HydroUnitsNoIceMeltNoStorageCapacity) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);
    basinSettings.AddHydroUnit(2, 50);
    basinSettings.AddLandCover("ground", "", 0.8);
    basinSettings.AddLandCover("glacier", "", 0.2);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    m_model.SelectHydroUnitBrick("glacier");
    m_model.SelectProcess("melt");
    m_model.SetProcessParameterValue("degree_day_factor", 0.0f);

    m_model.SelectHydroUnitBrick("slow_reservoir");
    m_model.SetBrickParameterValue("capacity", 0);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPet));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 80;
    double totalGlacierMelt = logger->GetTotalHydroUnits("glacier:melt:output");
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip - totalGlacierMelt;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelSocontBasic, WaterBalanceClosesWith2HydroUnitsNoIceMelt) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);
    basinSettings.AddHydroUnit(2, 50);
    basinSettings.AddLandCover("ground", "", 0.8);
    basinSettings.AddLandCover("glacier", "", 0.2);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    m_model.SelectHydroUnitBrick("glacier");
    m_model.SelectProcess("melt");
    m_model.SetProcessParameterValue("degree_day_factor", 0.0f);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPet));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 80;
    double totalGlacierMelt = logger->GetTotalHydroUnits("glacier:melt:output");
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip - totalGlacierMelt;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelSocontBasic, WaterBalanceClosesWith2HydroUnits) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);
    basinSettings.AddHydroUnit(2, 50);
    basinSettings.AddLandCover("ground", "", 0.2);
    basinSettings.AddLandCover("glacier", "", 0.8);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPet));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 80;
    double totalGlacierMelt = logger->GetTotalHydroUnits("glacier:melt:output");
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip - totalGlacierMelt;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelSocontBasic, WaterBalanceClosesWith4HydroUnits) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);
    basinSettings.AddHydroUnit(2, 50);
    basinSettings.AddLandCover("ground", "", 0.2);
    basinSettings.AddLandCover("glacier", "", 0.8);
    basinSettings.AddHydroUnit(3, 30);
    basinSettings.AddLandCover("ground", "", 0);
    basinSettings.AddLandCover("glacier", "", 1);
    basinSettings.AddHydroUnit(4, 200);
    basinSettings.AddLandCover("ground", "", 1);
    basinSettings.AddLandCover("glacier", "", 0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPet));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = 80;
    double totalGlacierMelt = logger->GetTotalHydroUnits("glacier:melt:output");
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = discharge + et + storage - precip - totalGlacierMelt;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST(ModelSocont, WaterBalanceCloses) {
    SettingsBasin basinSettings;
    EXPECT_TRUE(basinSettings.Parse("../../tests/files/catchments/ch_sitter_appenzell/hydro_units.nc"));

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);

    SettingsModel modelSettings;
    modelSettings.SetLogAll();
    modelSettings.SetSolver("heun_explicit");
    modelSettings.SetTimer("1981-01-01", "2020-12-31", 1, "day");
    modelSettings.SetLogAll(true);
    vecStr landCover = {"ground"};
    GenerateStructureSocont(modelSettings, landCover, landCover, 2, "linear_storage");
    modelSettings.SetParameter("slow_reservoir", "capacity", 200);

    EXPECT_TRUE(model.Initialize(modelSettings, basinSettings));
    EXPECT_TRUE(model.IsOk());

    std::vector<TimeSeries*> vecTimeSeries;
    EXPECT_TRUE(TimeSeries::Parse("../../tests/files/catchments/ch_sitter_appenzell/meteo.nc", vecTimeSeries));
    for (auto timeSeries : vecTimeSeries) {
        ASSERT_TRUE(model.AddTimeSeries(timeSeries));
    }
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = vecTimeSeries[0]->GetTotal(&basinSettings);
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();
    double snow = logger->GetTotalSnowStorageChanges();

    EXPECT_TRUE(storage >= 0);

    // Balance
    double balance = discharge + et + storage + snow - precip;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

class ModelSocontGletsch : public ::testing::Test {
  protected:
    SettingsModel m_model;
    TimeSeriesUniform* m_tsPrecip{};
    TimeSeriesUniform* m_tsTemp{};
    TimeSeriesUniform* m_tsPet{};

    void SetUp() override {
        m_model.SetLogAll();
        m_model.SetSolver("runge_kutta");
        m_model.SetTimer("2020-01-01", "2020-01-10", 1, "day");
        m_model.SetLogAll(true);

        vecStr landCoverTypes = {"ground"};
        vecStr landCoverNames = {"ground"};
        m_model.GenerateStructureSocont(landCoverTypes, landCoverNames, 1, "socont_runoff");

        auto precip = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        precip->SetValues({0.0, 13.8, 59.3, 34.2, 13.7, 26.1, 9.8, 0.0, 0.0, 0.0});
        m_tsPrecip = new TimeSeriesUniform(Precipitation);
        m_tsPrecip->SetData(precip);

        auto temperature = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        temperature->SetValues({5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0});
        m_tsTemp = new TimeSeriesUniform(Temperature);
        m_tsTemp->SetData(temperature);

        auto pet = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, Day);
        pet->SetValues({0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        m_tsPet = new TimeSeriesUniform(PET);
        m_tsPet->SetData(pet);
    }
    void TearDown() override {
        wxDELETE(m_tsPrecip);
        wxDELETE(m_tsTemp);
        wxDELETE(m_tsPet);
    }
};

TEST_F(ModelSocontGletsch, QuickDischargeIsCorrect) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 38.9 * 1000000);
    basinSettings.AddHydroUnitPropertyDouble("slope", 0.3, "m/m");
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    m_model.SetParameter("surface_runoff", "beta", 301);
    m_model.SetParameter("slow_reservoir", "capacity", 0);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model, basinSettings));
    EXPECT_TRUE(model.IsOk());

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsTemp));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPet));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Get discharge time series
    axd q = logger->GetOutletDischarge();

    // Cannot reproduce the exact same values as the solver is different
    EXPECT_NEAR(q[0], 0.0, 0.0000001);
    EXPECT_NEAR(q[1], 0.339098404522742, 0.000001);
    EXPECT_NEAR(q[2], 6.14339396606552, 0.000001);
    EXPECT_NEAR(q[3], 15.9871208705154, 0.000001);
    EXPECT_NEAR(q[4], 18.1637996149964, 0.000001);
    EXPECT_NEAR(q[5], 18.8240003953163, 0.000001);
    EXPECT_NEAR(q[6], 18.3973261932905, 0.000001);
    EXPECT_NEAR(q[7], 14.3377406789303, 0.000001);
    EXPECT_NEAR(q[8], 10.4729112050052, 0.000001);
    EXPECT_NEAR(q[9], 7.92405289891611, 0.000001);

    // Water balance components
    double precip = 156.9;
    double totalGlacierMelt = logger->GetTotalHydroUnits("glacier:melt:output");
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = precip + totalGlacierMelt - discharge - et - storage;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}