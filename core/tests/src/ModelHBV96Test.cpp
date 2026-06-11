#include <gtest/gtest.h>

#include <memory>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"
#include "helpers.h"

/**
 * Tests of the HBV-96 model structure (Lindström et al., 1997): beta-function
 * soil moisture routine, non-linear upper zone, linear lower zone, MAXBAS
 * triangular unit hydrograph and snow routine with liquid water retention and
 * refreezing.
 */
class ModelHBV96 : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;
    std::unique_ptr<TimeSeriesUniform> _tsPet;
    std::unique_ptr<TimeSeriesUniform> _tsTemp;

    void SetUp() override {
        _model.SetLogAll();
        _model.SetSolver("heun_explicit");
        _model.SetTimer("2020-01-01", "2020-01-15", 1, "day");

        auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 15), 1,
                                                              TimeUnit::Day);
        precip->SetValues({0.0, 0.0, 20.0, 20.0, 20.0, 20.0, 20.0, 20.0, 20.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        _tsPrecip->SetData(std::move(precip));

        auto pet = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 15), 1, TimeUnit::Day);
        pet->SetValues({1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0});
        _tsPet = std::make_unique<TimeSeriesUniform>(VariableType::PET);
        _tsPet->SetData(std::move(pet));

        // Warm temperatures: all the precipitation falls as rain.
        auto temp = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 15), 1, TimeUnit::Day);
        temp->SetValues({10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0});
        _tsTemp = std::make_unique<TimeSeriesUniform>(VariableType::Temperature);
        _tsTemp->SetData(std::move(temp));
    }

    void SetColdThenWarmTemperatures() {
        auto temp = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 15), 1, TimeUnit::Day);
        temp->SetValues({-10.0, -10.0, -10.0, -10.0, -10.0, -10.0, -10.0, -10.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0});
        _tsTemp = std::make_unique<TimeSeriesUniform>(VariableType::Temperature);
        _tsTemp->SetData(std::move(temp));
    }

    void SetFreezeThawTemperatures() {
        auto temp = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 15), 1, TimeUnit::Day);
        temp->SetValues({-5.0, -5.0, -5.0, 2.0, -5.0, 2.0, -5.0, 2.0, -5.0, 2.0, 2.0, -5.0, 2.0, 2.0, 2.0});
        _tsTemp = std::make_unique<TimeSeriesUniform>(VariableType::Temperature);
        _tsTemp->SetData(std::move(temp));
    }

    bool RunModel(ModelHydro& model) {
        EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
        EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
        EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
        EXPECT_TRUE(model.AttachTimeSeriesToHydroUnits());

        return static_cast<bool>(model.Run());
    }
};

TEST_F(ModelHBV96, ModelBuildsCorrectly) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureHBV96(_model);
    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());
}

TEST_F(ModelHBV96, ModelRunsCorrectly) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureHBV96(_model);
    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    EXPECT_TRUE(RunModel(model));
    EXPECT_GE(model.GetLogger()->GetOutletDischarge().minCoeff(), 0.0);
}

TEST_F(ModelHBV96, WaterBalanceClosesWithRain) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureHBV96(_model);
    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    EXPECT_TRUE(RunModel(model));

    Logger* logger = model.GetLogger();
    double precip = 140.0;  // 20 mm/d × 7 days
    double balance = logger->GetTotalOutletDischarge() + logger->GetTotalET() + logger->GetTotalWaterStorageChanges() +
                     logger->GetTotalSnowStorageChanges() - precip;
    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelHBV96, WaterBalanceClosesWithSnowAccumulation) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    SetColdThenWarmTemperatures();

    GenerateStructureHBV96(_model);
    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    EXPECT_TRUE(RunModel(model));

    Logger* logger = model.GetLogger();
    double precip = 140.0;  // 20 mm/d × 7 days
    double balance = logger->GetTotalOutletDischarge() + logger->GetTotalET() + logger->GetTotalWaterStorageChanges() +
                     logger->GetTotalSnowStorageChanges() - precip;
    EXPECT_NEAR(balance, 0.0, 0.0000001);
    EXPECT_GE(logger->GetOutletDischarge().minCoeff(), 0.0);
}

TEST_F(ModelHBV96, WaterBalanceClosesWithFreezeThawCycles) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    SetFreezeThawTemperatures();

    GenerateStructureHBV96(_model);
    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    EXPECT_TRUE(RunModel(model));

    Logger* logger = model.GetLogger();
    double precip = 140.0;  // 20 mm/d × 7 days
    double balance = logger->GetTotalOutletDischarge() + logger->GetTotalET() + logger->GetTotalWaterStorageChanges() +
                     logger->GetTotalSnowStorageChanges() - precip;
    EXPECT_NEAR(balance, 0.0, 0.0000001);
    EXPECT_GE(logger->GetOutletDischarge().minCoeff(), 0.0);
}

TEST_F(ModelHBV96, WaterBalanceClosesWithoutSnowRoutine) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureHBV96(_model, false);
    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();
    double precip = 140.0;  // 20 mm/d × 7 days
    double balance = logger->GetTotalOutletDischarge() + logger->GetTotalET() + logger->GetTotalWaterStorageChanges() -
                     precip;
    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelHBV96, MaxbasSpreadsTheDischarge) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureHBV96(_model);
    _model.SelectHydroUnitBrick("routing");
    _model.SelectProcess("routing");
    _model.SetProcessParameterValue("maxbas", 4.0f);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    EXPECT_TRUE(RunModel(model));

    Logger* logger = model.GetLogger();
    double precip = 140.0;  // 20 mm/d × 7 days
    double balance = logger->GetTotalOutletDischarge() + logger->GetTotalET() + logger->GetTotalWaterStorageChanges() +
                     logger->GetTotalSnowStorageChanges() - precip;
    EXPECT_NEAR(balance, 0.0, 0.0000001);
    EXPECT_GE(logger->GetOutletDischarge().minCoeff(), 0.0);
}

TEST_F(ModelHBV96, WaterBalanceClosesWithCapillaryFlux) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureHBV96(_model, false);
    _model.SelectHydroUnitBrick("upper_zone");
    _model.SelectProcess("capillary");
    _model.SetProcessParameterValue("max_capillary_flux", 1.0f);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();
    double precip = 140.0;  // 20 mm/d × 7 days
    double balance = logger->GetTotalOutletDischarge() + logger->GetTotalET() + logger->GetTotalWaterStorageChanges() -
                     precip;
    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelHBV96, WaterBalanceClosesWithCapillaryFluxAndLargeCapacity) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureHBV96(_model, false);
    _model.SelectHydroUnitBrick("soil_moisture");
    _model.SetBrickParameterValue("capacity", 2000.0f);
    _model.SelectHydroUnitBrick("upper_zone");
    _model.SelectProcess("capillary");
    _model.SetProcessParameterValue("max_capillary_flux", 1.0f);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();
    double precip = 140.0;  // 20 mm/d × 7 days
    double balance = logger->GetTotalOutletDischarge() + logger->GetTotalET() + logger->GetTotalWaterStorageChanges() -
                     precip;
    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelHBV96, RainOnColdSnowpackIsRetained) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    // Cold days build the snowpack, then T = 1 °C: with the linear transition (0-2 °C)
    // half of the precipitation falls as rain on a snowpack that does not melt
    // (melting temperature raised to 2 °C). The rain must be retained in the snowpack
    // liquid water storage (holding capacity > rain amount) and no discharge occur.
    auto temp = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 15), 1, TimeUnit::Day);
    temp->SetValues({-10.0, -10.0, -10.0, -10.0, -10.0, -10.0, -10.0, -10.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0});
    _tsTemp = std::make_unique<TimeSeriesUniform>(VariableType::Temperature);
    _tsTemp->SetData(std::move(temp));

    GenerateStructureHBV96(_model);
    _model.SelectHydroUnitBrick("ground_snowpack");
    _model.SelectProcess("melt");
    _model.SetProcessParameterValue("melting_temperature", 2.0f);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    EXPECT_TRUE(RunModel(model));

    Logger* logger = model.GetLogger();
    double precip = 140.0;  // 20 mm/d × 7 days
    double balance = logger->GetTotalOutletDischarge() + logger->GetTotalET() + logger->GetTotalWaterStorageChanges() +
                     logger->GetTotalSnowStorageChanges() - precip;
    EXPECT_NEAR(balance, 0.0, 0.0000001);
    EXPECT_NEAR(logger->GetTotalOutletDischarge(), 0.0, 0.0000001);
}

TEST_F(ModelHBV96, RainOnSnowpackDoesNotAlterRainOnlySimulations) {
    // Without snow cover, the rain routed through the snowpack liquid water storage
    // must reach the ground within the same time step: the discharge must be
    // identical with and without the rain-on-snowpack routing.
    auto runModel = [](bool rainOnSnowpack) {
        SettingsBasin basinSettings;
        basinSettings.AddHydroUnit(1, 100);
        basinSettings.AddLandCover("ground", "", 1.0);

        SubBasin subBasin;
        EXPECT_TRUE(subBasin.Initialize(basinSettings));

        SettingsModel settingsModel;
        settingsModel.SetLogAll();
        settingsModel.SetSolver("heun_explicit");
        settingsModel.SetTimer("2020-01-01", "2020-01-15", 1, "day");
        GenerateStructureHBV96(settingsModel, true, rainOnSnowpack);

        ModelHydro model(&subBasin);
        EXPECT_TRUE(model.Initialize(settingsModel, basinSettings));
        EXPECT_TRUE(model.IsValid());

        auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 15), 1,
                                                              TimeUnit::Day);
        precip->SetValues({0.0, 0.0, 20.0, 20.0, 20.0, 20.0, 20.0, 20.0, 20.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        auto tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        tsPrecip->SetData(std::move(precip));

        auto pet = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 15), 1, TimeUnit::Day);
        pet->SetValues({1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0});
        auto tsPet = std::make_unique<TimeSeriesUniform>(VariableType::PET);
        tsPet->SetData(std::move(pet));

        auto temp = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 15), 1, TimeUnit::Day);
        temp->SetValues({10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0});
        auto tsTemp = std::make_unique<TimeSeriesUniform>(VariableType::Temperature);
        tsTemp->SetData(std::move(temp));

        EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsPrecip))));
        EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsPet))));
        EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsTemp))));
        EXPECT_TRUE(model.AttachTimeSeriesToHydroUnits());
        EXPECT_TRUE(static_cast<bool>(model.Run()));

        return model.GetLogger()->GetOutletDischarge();
    };

    axd qRainOnSnowpack = runModel(true);
    axd qRainOnGround = runModel(false);

    ASSERT_EQ(qRainOnSnowpack.size(), qRainOnGround.size());
    for (int i = 0; i < qRainOnSnowpack.size(); ++i) {
        EXPECT_NEAR(qRainOnSnowpack[i], qRainOnGround[i], 0.0000001);
    }
}

TEST_F(ModelHBV96, NoPrecipitationProducesNoDischarge) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureHBV96(_model);
    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    auto precipValues = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 15), 1,
                                                                TimeUnit::Day);
    precipValues->SetValues({0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    auto tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
    tsPrecip->SetData(std::move(precipValues));
    _tsPrecip = std::move(tsPrecip);

    EXPECT_TRUE(RunModel(model));

    Logger* logger = model.GetLogger();
    double balance = logger->GetTotalOutletDischarge() + logger->GetTotalET() + logger->GetTotalWaterStorageChanges() +
                     logger->GetTotalSnowStorageChanges();
    EXPECT_NEAR(balance, 0.0, 0.0000001);
    EXPECT_NEAR(logger->GetTotalOutletDischarge(), 0.0, 0.0000001);
}
