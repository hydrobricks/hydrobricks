#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <memory>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"
#include "helpers.h"

class ModelSocontBasic : public ::testing::Test {
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
        GenerateStructureSocont(_model, landCoverTypes, landCoverNames, 2, "linear_storage");

        auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1,
                                                              TimeUnit::Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        _tsPrecip->SetData(std::move(precip));

        auto temperature = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1,
                                                                   TimeUnit::Day);
        temperature->SetValues({-2.0, -1.0, -1.0, 1.0, 2.0, 3.0, 4.0, 5.0, 8.0, 9.0});
        _tsTemp = std::make_unique<TimeSeriesUniform>(VariableType::Temperature);
        _tsTemp->SetData(std::move(temperature));

        auto pet = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, TimeUnit::Day);
        pet->SetValues({1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0});
        _tsPet = std::make_unique<TimeSeriesUniform>(VariableType::PET);
        _tsPet->SetData(std::move(pet));
    }
    void TearDown() override {
        // RAII cleanup via unique_ptr
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

    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());
}

TEST_F(ModelSocontBasic, ModelRunsCorrectly) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
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

    EXPECT_TRUE(model.Run());
}

TEST_F(ModelSocontBasic, WaterBalanceClosesWithoutGlacierMelt) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 0.5);
    basinSettings.AddLandCover("glacier", "", 0.5);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SelectHydroUnitBrick("glacier");
    _model.SelectProcess("melt");
    _model.SetProcessParameterValue("degree_day_factor", 0.0f);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
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
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    auto precipValues = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1,
                                                                TimeUnit::Day);
    precipValues->SetValues({0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    auto tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
    tsPrecip->SetData(std::move(precipValues));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
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
}

TEST_F(ModelSocontBasic, WaterBalanceClosesWithHeunExplicit) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
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
    _model.SetSolver("euler_explicit");
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
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
    _model.SetSolver("runge_kutta");
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
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
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    auto precipValues = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1,
                                                                TimeUnit::Day);
    precipValues->SetValues({0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    auto tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
    tsPrecip->SetData(std::move(precipValues));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
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

    _model.SelectHydroUnitBrick("glacier");
    _model.SelectProcess("melt");
    _model.SetProcessParameterValue("degree_day_factor", 0.0f);

    _model.SelectHydroUnitBrick("slow_reservoir");
    _model.SetBrickParameterValue("capacity", 0);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
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

    _model.SelectHydroUnitBrick("glacier");
    _model.SelectProcess("melt");
    _model.SetProcessParameterValue("degree_day_factor", 0.0f);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
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
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
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
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
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
    modelSettings.SetParameterValue("slow_reservoir", "capacity", 200);

    EXPECT_TRUE(model.Initialize(modelSettings, basinSettings));
    EXPECT_TRUE(model.IsValid());

    std::vector<std::unique_ptr<TimeSeries>> vecTimeSeries;
    EXPECT_TRUE(TimeSeries::Parse("../../tests/files/catchments/ch_sitter_appenzell/meteo.nc", vecTimeSeries));
    TimeSeries* firstTs = vecTimeSeries[0].get();
    for (auto& timeSeries : vecTimeSeries) {
        ASSERT_TRUE(model.AddTimeSeries(std::move(timeSeries)));
    }
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();

    // Water balance components
    double precip = firstTs->GetTotal(&basinSettings);
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();
    double snow = logger->GetTotalSnowStorageChanges();

    EXPECT_TRUE(storage >= 0);

    // Balance
    double balance = discharge + et + storage + snow - precip;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

class ModelSocontSingleLandCover : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;
    std::unique_ptr<TimeSeriesUniform> _tsTemp;
    std::unique_ptr<TimeSeriesUniform> _tsPet;

    void SetUp() override {
        _model.SetLogAll();
        _model.SetSolver("runge_kutta");
        _model.SetTimer("2020-01-01", "2020-01-10", 1, "day");
        _model.SetLogAll(true);

        vecStr landCoverTypes = {"ground"};
        vecStr landCoverNames = {"ground"};
        GenerateStructureSocont(_model, landCoverTypes, landCoverNames, 1, "socont_runoff");

        auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1,
                                                              TimeUnit::Day);
        precip->SetValues({0.0, 13.8, 59.3, 34.2, 13.7, 26.1, 9.8, 0.0, 0.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        _tsPrecip->SetData(std::move(precip));

        auto temperature = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1,
                                                                   TimeUnit::Day);
        temperature->SetValues({5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0});
        _tsTemp = std::make_unique<TimeSeriesUniform>(VariableType::Temperature);
        _tsTemp->SetData(std::move(temperature));

        auto pet = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, TimeUnit::Day);
        pet->SetValues({0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        _tsPet = std::make_unique<TimeSeriesUniform>(VariableType::PET);
        _tsPet->SetData(std::move(pet));
    }
    void TearDown() override {
        // RAII cleanup via unique_ptr
    }
};

TEST_F(ModelSocontSingleLandCover, QuickDischargeIsCorrect) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 38.9 * 1000000);
    basinSettings.AddHydroUnitPropertyDouble("slope", 0.3, "m/m");
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetParameterValue("surface_runoff", "beta", 301);
    _model.SetParameterValue("slow_reservoir", "capacity", 0);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
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

// Full Socont (slow reservoir + ET + quick flow together), single ground unit, run against a
// reference run of the original Matlab SOCONT (socontplusV2_6.m). Settings used in the reference:
//   area = 10 km2, slope = 0.3 m/m, A (slow capacity) = 2000 mm, beta = 300, lk = -9, PET = 1 mm/d.
// The reference outflow parameter is lk = log(k) with k in [1/h]; our internal response_factor is a
// rate in [1/d], so response_factor = 24 * exp(lk). The results cannot match exactly: the reference
// integrates each reservoir with its own clamped RK4 scheme, whereas hydrobricks uses a generic
// solver. We therefore only check that the discharge stays close to the reference, and that the
// water balance closes.
class ModelSocontReference : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;
    std::unique_ptr<TimeSeriesUniform> _tsTemp;
    std::unique_ptr<TimeSeriesUniform> _tsPet;

    void SetUp() override {
        _model.SetLogAll();
        _model.SetSolver("runge_kutta");
        _model.SetTimer("2020-01-01", "2020-02-19", 1, "day");  // 50 days
        _model.SetLogAll(true);

        vecStr landCoverTypes = {"ground"};
        vecStr landCoverNames = {"ground"};
        GenerateStructureSocont(_model, landCoverTypes, landCoverNames, 1, "socont_runoff");

        // Precipitation: 6 days of 50 mm (days 3-8), zero otherwise.
        vecDouble precipValues(50, 0.0);
        for (int i = 2; i < 8; ++i) {
            precipValues[i] = 50.0;
        }
        auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 2, 19), 1,
                                                              TimeUnit::Day);
        precip->SetValues(precipValues);
        _tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        _tsPrecip->SetData(std::move(precip));

        // Temperature is irrelevant here (no snow, no glacier); kept positive and constant.
        auto temperature = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 2, 19), 1,
                                                                   TimeUnit::Day);
        temperature->SetValues(vecDouble(50, 5.0));
        _tsTemp = std::make_unique<TimeSeriesUniform>(VariableType::Temperature);
        _tsTemp->SetData(std::move(temperature));

        // Constant PET of 1 mm/day.
        auto pet = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 2, 19), 1, TimeUnit::Day);
        pet->SetValues(vecDouble(50, 1.0));
        _tsPet = std::make_unique<TimeSeriesUniform>(VariableType::PET);
        _tsPet->SetData(std::move(pet));
    }
    void TearDown() override {
        // RAII cleanup via unique_ptr
    }
};

TEST_F(ModelSocontReference, DischargeIsCloseToReference) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 10.0 * 1000000);  // 10 km2
    basinSettings.AddHydroUnitPropertyDouble("slope", 0.3, "m/m");
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetParameterValue("surface_runoff", "beta", 300);
    _model.SetParameterValue("slow_reservoir", "capacity", 2000);                            // A
    _model.SetParameterValue("slow_reservoir", "response_factor", 24.0f * std::exp(-9.0f));  // 24 * exp(lk)

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();
    axd q = logger->GetOutletDischarge();

    // Reference outlet discharge (mm/day) from the original Matlab SOCONT (socontplusV2_6.m).
    vecDouble expected = {0.0,
                          0.0,
                          0.073863309448004,
                          0.221246178256454,
                          0.369818730346257,
                          0.524879737229557,
                          0.696844370213368,
                          0.901331076029334,
                          1.005437100459126,
                          0.986831145035943,
                          0.970406777743171,
                          0.955774714605604,
                          0.942627794554810,
                          0.930721260392261,
                          0.919858308138849,
                          0.909879364479211,
                          0.900654038556370,
                          0.892075016524970,
                          0.884053383966227,
                          0.876515009169802,
                          0.869397722613409,
                          0.862649099663166,
                          0.856224704341791,
                          0.850086688440947,
                          0.844202666636260,
                          0.838544807555746,
                          0.833089094989439,
                          0.827814724025129,
                          0.822703604847507,
                          0.817739952951539,
                          0.812909949101743,
                          0.808201455882595,
                          0.803603780398078,
                          0.799107474785951,
                          0.794704167859590,
                          0.790386422484919,
                          0.786147614323212,
                          0.781981828383324,
                          0.777883770475842,
                          0.773848691182148,
                          0.769872320370817,
                          0.765950810633217,
                          0.762080688286060,
                          0.758258810813770,
                          0.754482329807954,
                          0.750748658612861,
                          0.747055444010874,
                          0.743400541385659,
                          0.739781992886687,
                          0.736198008190589};

    ASSERT_EQ(q.size(), static_cast<int>(expected.size()));
    // Loose tolerance: the reference integrates the whole model with a single clamped RK4 scheme,
    // whereas hydrobricks routes water through separate bricks with a generic solver. The largest
    // deviation is on the first rain day, where the reference already produces outflow within the
    // step while here the infiltrated water reaches the slow reservoir (a static output) only at the
    // next step, so q stays 0 one step longer. The rising limb then agrees to a few 0.01 and the
    // recession to better than 1e-3.
    for (int i = 0; i < q.size(); ++i) {
        EXPECT_NEAR(q[i], expected[i], 0.1) << "at time step " << i;
    }

    // Water balance components
    double precip = 300.0;
    double totalGlacierMelt = logger->GetTotalHydroUnits("glacier:melt:output");
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double storage = logger->GetTotalWaterStorageChanges();

    // Balance
    double balance = precip + totalGlacierMelt - discharge - et - storage;

    EXPECT_NEAR(balance, 0.0, 0.0000001);
}
