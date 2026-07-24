#include <gtest/gtest.h>

#include <memory>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"
#include "helpers.h"

class ModelGR6JBasic : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;
    std::unique_ptr<TimeSeriesUniform> _tsPet;

    void SetUp() override {
        _model.SetLogAll();
        _model.SetSolver("heun_explicit");
        _model.SetTimer("2020-01-01", "2020-01-15", 1, "day");

        auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 15), 1,
                                                              TimeUnit::Day);
        precip->SetValues({0.0, 0.0, 50.0, 50.0, 50.0, 50.0, 50.0, 50.0, 50.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        _tsPrecip->SetData(std::move(precip));

        auto pet = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 15), 1, TimeUnit::Day);
        pet->SetValues({1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0});
        _tsPet = std::make_unique<TimeSeriesUniform>(VariableType::PET);
        _tsPet->SetData(std::move(pet));
    }
};

TEST_F(ModelGR6JBasic, ModelBuildsCorrectly) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR6J(_model);
    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());
}

TEST_F(ModelGR6JBasic, ModelRunsCorrectly) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR6J(_model);
    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
}

TEST_F(ModelGR6JBasic, WaterBalanceClosesX2Zero) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR6J(_model);
    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();
    double totalPrecip = 350.0;  // 50 mm/d × 7 days
    double balance = logger->GetTotalOutletDischarge() + logger->GetTotalET() + logger->GetTotalWaterStorageChanges() -
                     totalPrecip;
    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelGR6JBasic, WaterBalanceClosesX2ZeroWithSolver) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR6J(_model, false);
    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();
    double totalPrecip = 350.0;  // 50 mm/d × 7 days
    double balance = logger->GetTotalOutletDischarge() + logger->GetTotalET() + logger->GetTotalWaterStorageChanges() -
                     totalPrecip;
    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelGR6JBasic, WaterBalanceClosesNoPrecip) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR6J(_model);
    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    auto precipValues = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 15), 1,
                                                                TimeUnit::Day);
    precipValues->SetValues({0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    auto tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
    tsPrecip->SetData(std::move(precipValues));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();
    double balance = logger->GetTotalOutletDischarge() + logger->GetTotalET() + logger->GetTotalWaterStorageChanges();
    EXPECT_NEAR(balance, 0.0, 0.0000001);
    EXPECT_GE(logger->GetOutletDischarge().minCoeff(), 0.0);
}

TEST_F(ModelGR6JBasic, WaterBalanceClosesNoPET) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR6J(_model);
    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    auto petValues = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 15), 1, TimeUnit::Day);
    petValues->SetValues({0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    auto tsPet = std::make_unique<TimeSeriesUniform>(VariableType::PET);
    tsPet->SetData(std::move(petValues));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();
    double totalPrecip = 350.0;  // 50 mm/d × 7 days
    double balance = logger->GetTotalOutletDischarge() + logger->GetTotalET() + logger->GetTotalWaterStorageChanges() -
                     totalPrecip;
    EXPECT_NEAR(balance, 0.0, 0.0000001);
}

TEST_F(ModelGR6JBasic, PositiveExchangeDischargeNonNegative) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR6J(_model);
    _model.SelectHydroUnitBrick("uh_input");
    _model.SelectProcess("routing");
    _model.SetProcessParameterValue("exchange_factor", 1.0f);  // X2

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
    EXPECT_GE(model.GetLogger()->GetOutletDischarge().minCoeff(), 0.0);
}

TEST_F(ModelGR6JBasic, NegativeExchangeDischargeNonNegative) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR6J(_model);
    _model.SelectHydroUnitBrick("uh_input");
    _model.SelectProcess("routing");
    _model.SetProcessParameterValue("exchange_factor", -3.0f);  // X2

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
    EXPECT_GE(model.GetLogger()->GetOutletDischarge().minCoeff(), 0.0);
}

TEST_F(ModelGR6JBasic, SmallX4Works) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR6J(_model);
    _model.SelectHydroUnitBrick("uh_input");
    _model.SelectProcess("routing");
    _model.SetProcessParameterValue("uh_base_time", 0.51f);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
    EXPECT_GE(model.GetLogger()->GetOutletDischarge().minCoeff(), 0.0);
}

TEST_F(ModelGR6JBasic, SmallX6Works) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR6J(_model);
    _model.SelectHydroUnitBrick("uh_input");
    _model.SelectProcess("routing");
    _model.SetProcessParameterValue("exp_store_coeff", 0.05f);  // X6 minimum

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
    EXPECT_GE(model.GetLogger()->GetOutletDischarge().minCoeff(), 0.0);
}

// ---------------------------------------------------------------------------
// Reference verification (shared with python/tests/test_gr6j.py).
// Forcing: same as the fixture; initial state S=0, R=0, Rexp=0.
// ---------------------------------------------------------------------------

namespace {
void RunGR6J(SettingsModel& model, float X1, float X2, float X3, float X4, float X5, float X6,
             std::unique_ptr<TimeSeriesUniform> tsPrecip, std::unique_ptr<TimeSeriesUniform> tsPet, axd& discharge) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    ASSERT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR6J(model);
    model.SelectHydroUnitBrick("production_store");
    model.SetBrickParameterValue("capacity", X1);
    model.SelectHydroUnitBrick("uh_input");
    model.SelectProcess("routing");
    model.SetProcessParameterValue("exchange_factor", X2);
    model.SetProcessParameterValue("routing_capacity", X3);
    model.SetProcessParameterValue("uh_base_time", X4);
    model.SetProcessParameterValue("exchange_threshold", X5);
    model.SetProcessParameterValue("exp_store_coeff", X6);

    ModelHydro hydro(&subBasin);
    ASSERT_TRUE(hydro.Initialize(model, basinSettings));
    ASSERT_TRUE(hydro.IsValid());
    ASSERT_TRUE(hydro.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsPrecip))));
    ASSERT_TRUE(hydro.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsPet))));
    ASSERT_TRUE(hydro.AttachTimeSeriesToHydroUnits());
    ASSERT_TRUE(hydro.Run());
    discharge = hydro.GetLogger()->GetOutletDischarge();
}
}  // namespace

// Reference outlet discharge (mm) from the airGR RunModel_GR6J implementation
// (Pushpalatha et al., 2011), with X1=300, X3=100, X4=2, X5=0.3, X6=4 and the
// fixture forcing; initial state S=0, R=0, Rexp=0. The three cases differ only by
// the groundwater exchange coefficient X2.
TEST_F(ModelGR6JBasic, GivesSameResultsAsAirGRNoExchange) {
    axd q;
    RunGR6J(_model, 300.0f, 0.0f, 100.0f, 2.0f, 0.3f, 4.0f, std::move(_tsPrecip), std::move(_tsPet), q);

    vecDouble expected = {2.772589,  1.621860,  1.161424, 1.005298, 1.249373, 2.151582, 4.214921, 7.687066,
                          11.521525, 12.829463, 5.323870, 3.072099, 2.322168, 2.032455, 1.833531};
    ASSERT_EQ(q.size(), static_cast<int>(expected.size()));
    for (int i = 0; i < q.size(); ++i) {
        EXPECT_NEAR(q[i], expected[i], 1e-5) << "at time step " << i;
    }
}

TEST_F(ModelGR6JBasic, GivesSameResultsAsAirGRPositiveExchange) {
    axd q;
    RunGR6J(_model, 300.0f, 2.0f, 100.0f, 2.0f, 0.3f, 4.0f, std::move(_tsPrecip), std::move(_tsPet), q);

    vecDouble expected = {2.483828,  1.340565,  0.882276, 0.676503, 0.686399, 1.028097, 2.754801, 6.301642,
                          10.888971, 12.851495, 5.795915, 3.611188, 2.918015, 2.678096, 2.523144};
    ASSERT_EQ(q.size(), static_cast<int>(expected.size()));
    for (int i = 0; i < q.size(); ++i) {
        EXPECT_NEAR(q[i], expected[i], 1e-5) << "at time step " << i;
    }
}

TEST_F(ModelGR6JBasic, GivesSameResultsAsAirGRNegativeExchange) {
    axd q;
    RunGR6J(_model, 300.0f, -2.0f, 100.0f, 2.0f, 0.3f, 4.0f, std::move(_tsPrecip), std::move(_tsPet), q);

    vecDouble expected = {3.683828,  2.523956,  2.055770, 1.908026, 2.217910, 3.269898, 5.447277, 8.589649,
                          11.827181, 12.701670, 4.808356, 2.631865, 2.088443, 1.754517, 1.522078};
    ASSERT_EQ(q.size(), static_cast<int>(expected.size()));
    for (int i = 0; i < q.size(); ++i) {
        EXPECT_NEAR(q[i], expected[i], 1e-5) << "at time step " << i;
    }
}
