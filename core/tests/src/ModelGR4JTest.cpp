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

TEST_F(ModelGR4JBasic, ModelBuildsCorrectly) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR4J(_model);
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

    GenerateStructureGR4J(_model);
    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
}

TEST_F(ModelGR4JBasic, WaterBalanceClosesX2Zero) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR4J(_model);
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

TEST_F(ModelGR4JBasic, WaterBalanceClosesNoPrecip) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR4J(_model);
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

TEST_F(ModelGR4JBasic, WaterBalanceClosesNoPET) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR4J(_model);
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

TEST_F(ModelGR4JBasic, PositiveExchangeRunsSuccessfully) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR4J(_model);
    _model.SelectHydroUnitBrick("uh_input");
    _model.SelectProcess("routing");
    _model.SetProcessParameterValue("exchange_factor", 1.0f);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
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

    GenerateStructureGR4J(_model);
    _model.SelectHydroUnitBrick("uh_input");
    _model.SelectProcess("routing");
    _model.SetProcessParameterValue("exchange_factor", -5.0f);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
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

    GenerateStructureGR4J(_model);
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

TEST_F(ModelGR4JBasic, LargeX1BalanceCloses) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR4J(_model);
    _model.SelectHydroUnitBrick("production_store");
    _model.SetBrickParameterValue("capacity", 1000.0f);

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

TEST_F(ModelGR4JBasic, SmallX3Works) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR4J(_model);
    _model.SelectHydroUnitBrick("uh_input");
    _model.SelectProcess("routing");
    _model.SetProcessParameterValue("routing_capacity", 1.0f);

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
    EXPECT_GE(model.GetLogger()->GetOutletDischarge().minCoeff(), 0.0);
}

TEST_F(ModelGR4JBasic, GivesSameResultsAsReferenceNoExchange) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR4J(_model);
    _model.SelectHydroUnitBrick("production_store");
    _model.SetBrickParameterValue("capacity", 300.0f);  // X1
    _model.SelectHydroUnitBrick("uh_input");
    _model.SelectProcess("routing");
    _model.SetProcessParameterValue("exchange_factor", 0.0f);     // X2
    _model.SetProcessParameterValue("routing_capacity", 100.0f);  // X3
    _model.SetProcessParameterValue("uh_base_time", 2.0f);        // X4

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Reference outlet discharge (mm) from a reference GR4J implementation.
    vecDouble expected = {0.000000, 0.000000,  0.003814, 0.043580, 0.203829, 0.548403, 1.074379, 2.015970,
                          5.228185, 11.837592, 7.648482, 4.872998, 3.672856, 3.060546, 2.629808};
    axd q = model.GetLogger()->GetOutletDischarge();
    ASSERT_EQ(q.size(), static_cast<int>(expected.size()));
    for (int i = 0; i < q.size(); ++i) {
        EXPECT_NEAR(q[i], expected[i], 0.0000005) << "at time step " << i;
    }
}

TEST_F(ModelGR4JBasic, SolverGetsCloseToReferenceNoExchange) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR4J(_model, false);
    _model.SelectHydroUnitBrick("production_store");
    _model.SetBrickParameterValue("capacity", 300.0f);  // X1
    _model.SelectHydroUnitBrick("uh_input");
    _model.SelectProcess("routing");
    _model.SetProcessParameterValue("exchange_factor", 0.0f);     // X2
    _model.SetProcessParameterValue("routing_capacity", 100.0f);  // X3
    _model.SetProcessParameterValue("uh_base_time", 2.0f);        // X4

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Reference outlet discharge (mm) from a reference GR4J implementation.
    vecDouble expected = {0.0, 0.0, 0.0038, 0.044, 0.204, 0.548, 1.07, 2.02, 5.23, 11.8, 7.65, 4.87, 3.67, 3.06, 2.63};
    axd q = model.GetLogger()->GetOutletDischarge();
    ASSERT_EQ(q.size(), static_cast<int>(expected.size()));
    for (int i = 0; i < q.size(); ++i) {
        // Large tolerance as not expected to be directly comparable.
        EXPECT_NEAR(q[i], expected[i], expected[i] * 0.1) << "at time step " << i;
    }
}

TEST_F(ModelGR4JBasic, GivesSameResultsAsReferencePosExchange) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR4J(_model);
    _model.SelectHydroUnitBrick("production_store");
    _model.SetBrickParameterValue("capacity", 200.0f);  // X1
    _model.SelectHydroUnitBrick("uh_input");
    _model.SelectProcess("routing");
    _model.SetProcessParameterValue("exchange_factor", 2.0f);    // X2
    _model.SetProcessParameterValue("routing_capacity", 90.0f);  // X3
    _model.SetProcessParameterValue("uh_base_time", 1.7f);       // X4

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Reference outlet discharge (mm) from a reference GR4J implementation.
    vecDouble expected = {0.000000,  0.000000,  0.012725,  0.136420, 0.573964, 1.373628, 3.260256, 11.690935,
                          29.438060, 33.338379, 14.274200, 8.309123, 6.095194, 4.869705, 4.080997};
    axd q = model.GetLogger()->GetOutletDischarge();
    ASSERT_EQ(q.size(), static_cast<int>(expected.size()));
    for (int i = 0; i < q.size(); ++i) {
        EXPECT_NEAR(q[i], expected[i], 0.0000006) << "at time step " << i;
    }
}

TEST_F(ModelGR4JBasic, GivesSameResultsAsReferenceNegExchange) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    GenerateStructureGR4J(_model);
    _model.SelectHydroUnitBrick("production_store");
    _model.SetBrickParameterValue("capacity", 200.0f);  // X1
    _model.SelectHydroUnitBrick("uh_input");
    _model.SelectProcess("routing");
    _model.SetProcessParameterValue("exchange_factor", -3.0f);   // X2
    _model.SetProcessParameterValue("routing_capacity", 90.0f);  // X3
    _model.SetProcessParameterValue("uh_base_time", 1.7f);       // X4

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(_model, basinSettings));
    EXPECT_TRUE(model.IsValid());

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPet))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Reference outlet discharge (mm) from a reference GR4J implementation.
    vecDouble expected = {0.000000,  0.000000,  0.012725, 0.136420, 0.573950, 1.371315, 3.193552, 10.875901,
                          25.635053, 26.849073, 9.237705, 5.488689, 3.862539, 2.968519, 2.407908};
    axd q = model.GetLogger()->GetOutletDischarge();
    ASSERT_EQ(q.size(), static_cast<int>(expected.size()));
    for (int i = 0; i < q.size(); ++i) {
        EXPECT_NEAR(q[i], expected[i], 0.0000007) << "at time step " << i;
    }
}
