#include <gtest/gtest.h>

#include <memory>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"
#include "helpers.h"

class ModelGR4JBasic : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;
    std::unique_ptr<TimeSeriesUniform> _tsPet;
    std::unique_ptr<TimeSeriesUniform> _tsTemp;

    void SetUp() override {
        _model.SetLogAll();
        _model.SetSolver("heun_explicit");
        _model.SetTimer("2020-01-01", "2020-01-10", 1, "day");
        GenerateStructureGR4J(_model);

        auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1,
                                                              TimeUnit::Day);
        precip->SetValues({5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 0.0, 0.0, 0.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        _tsPrecip->SetData(std::move(precip));

        auto pet = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, TimeUnit::Day);
        pet->SetValues({3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0});
        _tsPet = std::make_unique<TimeSeriesUniform>(VariableType::PET);
        _tsPet->SetData(std::move(pet));

        // T=10°C ensures all precipitation falls as rain (above CemaNeige rain/snow threshold)
        auto temperature = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1,
                                                                   TimeUnit::Day);
        temperature->SetValues({10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0});
        _tsTemp = std::make_unique<TimeSeriesUniform>(VariableType::Temperature);
        _tsTemp->SetData(std::move(temperature));
    }
};

TEST_F(ModelGR4JBasic, ModelBuildsCorrectly) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());
}

TEST_F(ModelGR4JBasic, ModelRunsCorrectly) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
}

TEST_F(ModelGR4JBasic, WaterBalanceClosesX2Zero) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();
    double totalPrecip = 50.0;  // 5 mm/d × 10 days
    double balance = logger->GetTotalOutletDischarge() + logger->GetTotalET() + logger->GetTotalWaterStorageChanges() -
                     totalPrecip;
    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelGR4JBasic, WaterBalanceClosesNoPrecip) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

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
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();
    double balance = logger->GetTotalOutletDischarge() + logger->GetTotalET() + logger->GetTotalWaterStorageChanges();
    EXPECT_NEAR(balance, 0.0, 0.0000001);
    EXPECT_GE(logger->GetOutletDischarge().minCoeff(), 0.0);
}

TEST_F(ModelGR4JBasic, WaterBalanceClosesNoPET) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    auto petValues = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, TimeUnit::Day);
    petValues->SetValues({0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    auto tsPet = std::make_unique<TimeSeriesUniform>(VariableType::PET);
    tsPet->SetData(std::move(petValues));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsPet))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();
    double totalPrecip = 50.0;  // 5 mm/d × 10 days
    double balance = logger->GetTotalOutletDischarge() + logger->GetTotalET() + logger->GetTotalWaterStorageChanges() -
                     totalPrecip;
    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelGR4JBasic, PositiveExchangeRunsSuccessfully) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SelectHydroUnitBrick("uh_input");
    _model.SelectProcess("routing");
    _model.SetProcessParameterValue("exchange_factor", 1.0f);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
    EXPECT_GE(model.GetLogger()->GetOutletDischarge().minCoeff(), 0.0);
}

TEST_F(ModelGR4JBasic, NegativeExchangeDischargeNonNegative) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SelectHydroUnitBrick("uh_input");
    _model.SelectProcess("routing");
    _model.SetProcessParameterValue("exchange_factor", -5.0f);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
    EXPECT_GE(model.GetLogger()->GetOutletDischarge().minCoeff(), 0.0);
}

TEST_F(ModelGR4JBasic, SmallX4Works) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SelectHydroUnitBrick("uh_input");
    _model.SelectProcess("routing");
    _model.SetProcessParameterValue("uh_base_time", 0.51f);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
    EXPECT_GE(model.GetLogger()->GetOutletDischarge().minCoeff(), 0.0);
}

TEST_F(ModelGR4JBasic, LargeX1BalanceCloses) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SelectHydroUnitBrick("production_store");
    _model.SetBrickParameterValue("capacity", 1000.0f);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();
    double totalPrecip = 50.0;  // 5 mm/d × 10 days
    double balance = logger->GetTotalOutletDischarge() + logger->GetTotalET() + logger->GetTotalWaterStorageChanges() -
                     totalPrecip;
    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelGR4JBasic, SmallX3Works) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SelectHydroUnitBrick("uh_input");
    _model.SelectProcess("routing");
    _model.SetProcessParameterValue("routing_capacity", 1.0f);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsTemp))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
    EXPECT_GE(model.GetLogger()->GetOutletDischarge().minCoeff(), 0.0);
}
