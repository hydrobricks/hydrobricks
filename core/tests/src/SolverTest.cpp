#include <gtest/gtest.h>

#include <memory>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

TEST(Solver, FactoryBuildsSolvers) {
    SolverSettings settings;

    settings.name = "rk4";
    EXPECT_TRUE(Solver::Factory(settings) != nullptr);

    settings.name = "runge_kutta";
    EXPECT_TRUE(Solver::Factory(settings) != nullptr);

    settings.name = "runge_kutta";
    EXPECT_TRUE(Solver::Factory(settings) != nullptr);

    settings.name = "euler_explicit";
    EXPECT_TRUE(Solver::Factory(settings) != nullptr);

    settings.name = "heun_explicit";
    EXPECT_TRUE(Solver::Factory(settings) != nullptr);

    settings.name = "analytic_linear";
    EXPECT_TRUE(Solver::Factory(settings) != nullptr);

    settings.name = "analytic";
    EXPECT_TRUE(Solver::Factory(settings) != nullptr);

    settings.name = "implicit_euler";
    EXPECT_TRUE(Solver::Factory(settings) != nullptr);

    settings.name = "euler_implicit";
    EXPECT_TRUE(Solver::Factory(settings) != nullptr);
}

TEST(Solver, FactoryThrowsExceptionIfNameInvalid) {
    SolverSettings settings;

    settings.name = "invalid_name";
    EXPECT_THROW(Solver::Factory(settings), ModelConfigError);
}

TEST(Solver, RejectsSolvedBrickFeedingDirectBrick) {
    SettingsModel settings;
    settings.SetSolver("euler_explicit");
    settings.SetTimer("2020-01-01", "2020-01-10", 1, "day");

    // A solved storage sending water to a brick computed directly is invalid:
    // direct bricks are processed before the solver runs.
    settings.AddHydroUnitBrick("storage", "storage");
    settings.AddBrickForcing("precipitation");
    settings.AddBrickProcess("outflow", "outflow:linear", "transfer");

    settings.AddHydroUnitBrick("transfer", "storage");
    settings.SetCurrentBrickComputedDirectly();
    settings.AddBrickProcess("outflow", "outflow:direct", "outlet");

    settings.AddLoggingToItem("outlet");

    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    auto result = model.Initialize(settings, basinSettings);
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().find("computed directly") != string::npos);
}

TEST(Solver, AcceptsDirectBrickDeclaredAfterSolvedBrick) {
    SettingsModel settings;
    settings.SetSolver("euler_explicit");
    settings.SetTimer("2020-01-01", "2020-01-10", 1, "day");

    // A brick computed directly declared after a solved brick is fine as long as it
    // does not receive water from it (classification is a brick property, not a
    // declaration-order rule).
    settings.AddHydroUnitBrick("storage", "storage");
    settings.AddBrickForcing("precipitation");
    settings.AddBrickProcess("outflow", "outflow:linear", "outlet");

    settings.AddHydroUnitBrick("transfer", "storage");
    settings.SetCurrentBrickComputedDirectly();
    settings.AddBrickForcing("precipitation");
    settings.AddBrickProcess("outflow", "outflow:direct", "outlet");

    settings.AddLoggingToItem("outlet");

    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(settings, basinSettings));
}

/**
 * Model: simple linear storage
 */
class SolverLinearStorage : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;

    void SetUp() override {
        _model.SetSolver("euler_explicit");
        _model.SetTimer("2020-01-01", "2020-01-20", 1, "day");

        // Main storage
        _model.AddHydroUnitBrick("storage", "storage");
        _model.AddBrickForcing("precipitation");
        _model.AddBrickLogging("water_content");
        _model.AddBrickProcess("outflow", "outflow:linear");
        _model.SetProcessParameterValue("response_factor", 0.3f);
        _model.AddProcessLogging("output");
        _model.AddProcessOutput("outlet");

        _model.AddLoggingToItem("outlet");

        auto data = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 20), 1, TimeUnit::Day);
        data->SetValues(
            {0.0, 10.0, 10.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        _tsPrecip->SetData(std::move(data));
    }
    void TearDown() override {
        // RAII cleanup via unique_ptr
    }
};

TEST_F(SolverLinearStorage, UsingEulerExplicit) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetSolver("euler_explicit");

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble expectedOutputs = {0.000000, 0.000000, 3.000000, 5.100000, 6.570000, 4.599000, 3.219300,
                                 2.253510, 1.577457, 1.104220, 0.772954, 0.541068, 0.378747, 0.265123,
                                 0.185586, 0.129910, 0.090937, 0.063656, 0.044559, 0.031191};

    for (auto& basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }

    // Check water balance
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    double storageContent = unitContent[0](19, 0);
    EXPECT_NEAR(storageContent, 0.072780, 0.000001);
    EXPECT_NEAR(30.0 - basinOutputs[0].sum() - storageContent, 0, 0.00000000000001);
}

TEST_F(SolverLinearStorage, UsingHeunExplicit) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetSolver("heun_explicit");

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble expectedOutputs = {0.000000, 1.500000, 3.667500, 5.282288, 4.985304, 3.714052, 2.766968,
                                 2.061392, 1.535737, 1.144124, 0.852372, 0.635017, 0.473088, 0.352450,
                                 0.262576, 0.195619, 0.145736, 0.108573, 0.080887, 0.060261};

    for (auto& basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }

    // Check water balance
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    double storageContent = unitContent[0](19, 0);
    EXPECT_NEAR(storageContent, 0.176056, 0.000001);
    EXPECT_NEAR(30.0 - basinOutputs[0].sum() - storageContent, 0, 0.00000000000001);
}

TEST_F(SolverLinearStorage, UsingRungeKutta) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetSolver("runge_kutta");

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble expectedOutputs = {0.000000, 1.361250, 3.600090, 5.258707, 5.126222, 3.797698, 2.813477,
                                 2.084329, 1.544149, 1.143964, 0.847491, 0.627853, 0.465137, 0.344591,
                                 0.255286, 0.189125, 0.140111, 0.103800, 0.076899, 0.056969};

    for (auto& basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }

    // Check water balance
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    double storageContent = unitContent[0](19, 0);
    EXPECT_NEAR(storageContent, 0.162852, 0.000001);
    EXPECT_NEAR(30.0 - basinOutputs[0].sum() - storageContent, 0, 0.00000000000001);
}

TEST_F(SolverLinearStorage, UsingAnalyticLinear) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetSolver("analytic_linear");

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge against the closed-form solution of dS/dt = I - k S
    // with the inflow constant over each step.
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble precip = {0.0, 10.0, 10.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                        0.0, 0.0,  0.0,  0.0,  0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double k = 0.3f;  // Value stored as float in the parameter
    double decay = std::exp(-k);
    double storage = 0.0;

    ASSERT_EQ(basinOutputs[0].size(), precip.size());
    for (int j = 0; j < basinOutputs[0].size(); ++j) {
        double newStorage = storage * decay + precip[j] / k * (1.0 - decay);
        double outflow = precip[j] - (newStorage - storage);
        EXPECT_NEAR(basinOutputs[0][j], outflow, 0.000000001);
        storage = newStorage;
    }

    // The discharge responds within the first precipitation step (no lag).
    EXPECT_GT(basinOutputs[0][1], 0.0);

    // Check water balance
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    double storageContent = unitContent[0](19, 0);
    EXPECT_NEAR(storageContent, storage, 0.000000001);
    EXPECT_NEAR(30.0 - basinOutputs[0].sum() - storageContent, 0, 0.000000001);
}

TEST_F(SolverLinearStorage, UsingImplicitEuler) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetSolver("implicit_euler");

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge against the closed form of the implicit step for a
    // linear storage: S(t+h) = (S(t) + I h) / (1 + k h), outflow = k S(t+h) h.
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble precip = {0.0, 10.0, 10.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                        0.0, 0.0,  0.0,  0.0,  0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double k = 0.3f;  // Value stored as float in the parameter
    double storage = 0.0;

    ASSERT_EQ(basinOutputs[0].size(), precip.size());
    for (int j = 0; j < basinOutputs[0].size(); ++j) {
        double newStorage = (storage + precip[j]) / (1.0 + k);
        double outflow = k * newStorage;
        EXPECT_NEAR(basinOutputs[0][j], outflow, 0.00000001);
        storage = newStorage;
    }

    // The discharge responds within the first precipitation step (no lag).
    EXPECT_GT(basinOutputs[0][1], 0.0);

    // Check water balance
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    double storageContent = unitContent[0](19, 0);
    EXPECT_NEAR(storageContent, storage, 0.00000001);
    EXPECT_NEAR(30.0 - basinOutputs[0].sum() - storageContent, 0, 0.00000001);
}

TEST_F(SolverLinearStorage, ImplicitEulerIsStableForFastReservoir) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    // k h = 2.5: explicit Euler diverges (stability limit k h = 2); the implicit
    // scheme must remain stable and match its closed form.
    _model.SetSolver("implicit_euler");
    _model.SelectHydroUnitBrick("storage");
    _model.SelectProcess("outflow");
    _model.SetProcessParameterValue("response_factor", 2.5f);

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble precip = {0.0, 10.0, 10.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                        0.0, 0.0,  0.0,  0.0,  0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double k = 2.5f;
    double storage = 0.0;

    for (int j = 0; j < basinOutputs[0].size(); ++j) {
        double newStorage = (storage + precip[j]) / (1.0 + k);
        double outflow = k * newStorage;
        // Tolerance above the round-off snapping of near-empty containers, which
        // cuts the infinite exponential tail to exactly zero.
        EXPECT_NEAR(basinOutputs[0][j], outflow, 0.000001);
        EXPECT_GE(basinOutputs[0][j], 0.0);
        storage = newStorage;
    }
}

/**
 * Model: 2 linear storages in cascade
 */
class Solver2LinearStorages : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;

    void SetUp() override {
        _model.SetSolver("euler_explicit");
        _model.SetTimer("2020-01-01", "2020-01-20", 1, "day");

        // First storage
        _model.AddHydroUnitBrick("storage_1", "storage");
        _model.AddBrickForcing("precipitation");
        _model.AddBrickLogging("water_content");
        _model.AddBrickProcess("outflow", "outflow:linear");
        _model.SetProcessParameterValue("response_factor", 0.5f);
        _model.AddProcessLogging("output");
        _model.AddProcessOutput("storage_2");

        // Second storage
        _model.AddHydroUnitBrick("storage_2", "storage");
        _model.AddBrickLogging("water_content");
        _model.AddBrickProcess("outflow", "outflow:linear");
        _model.SetProcessParameterValue("response_factor", 0.3f);
        _model.AddProcessLogging("output");
        _model.AddProcessOutput("outlet");

        _model.AddLoggingToItem("outlet");

        auto data = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 20), 1, TimeUnit::Day);
        data->SetValues(
            {0.0, 10.0, 10.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        _tsPrecip->SetData(std::move(data));
    }
    void TearDown() override {
        // RAII cleanup via unique_ptr
    }
};

TEST_F(Solver2LinearStorages, UsingEulerExplicit) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetSolver("euler_explicit");

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble expectedOutputs = {0.000000, 0.000000, 0.000000, 1.500000, 3.300000, 4.935000, 4.767000,
                                 3.993150, 3.123330, 2.350394, 1.727307, 1.250130, 0.895599, 0.637173,
                                 0.451148, 0.318367, 0.224139, 0.157538, 0.110597, 0.077578};

    for (auto& basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }

    // Check water balance
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    double contentStorage1 = unitContent[0](19, 0);
    EXPECT_NEAR(contentStorage1, 0.000267, 0.000001);
    double contentStorage2 = unitContent[2](19, 0);
    EXPECT_NEAR(contentStorage2, 0.181283, 0.000001);
    EXPECT_NEAR(30.0 - basinOutputs[0].sum() - contentStorage1 - contentStorage2, 0, 0.00000000000001);
}

TEST_F(Solver2LinearStorages, UsingHeunExplicit) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetSolver("heun_explicit");

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble expectedOutputs = {0.000000, 0.000000, 1.200000, 2.600250, 3.959843, 3.970493, 3.595773,
                                 3.077449, 2.541823, 2.049360, 1.624087, 1.270766, 0.984734, 0.757385,
                                 0.579101, 0.440711, 0.334130, 0.252552, 0.190417, 0.143277};

    for (auto& basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }

    // Check water balance
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    double contentStorage1 = unitContent[0](19, 0);
    EXPECT_NEAR(contentStorage1, 0.008195, 0.000001);
    double contentStorage2 = unitContent[2](19, 0);
    EXPECT_NEAR(contentStorage2, 0.419653, 0.000001);
    EXPECT_NEAR(30.0 - basinOutputs[0].sum() - contentStorage1 - contentStorage2, 0, 0.00000000000001);
}

TEST_F(Solver2LinearStorages, UsingRungeKutta) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetSolver("runge_kutta");

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble expectedOutputs = {0.000000, 0.200000, 1.158225, 2.490032, 3.654047, 3.935308, 3.660692,
                                 3.164185, 2.618533, 2.106397, 1.661518, 1.292212, 0.994512, 0.759339,
                                 0.576240, 0.435209, 0.327461, 0.245654, 0.183846, 0.137326};

    for (auto& basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }

    // Check water balance
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    double contentStorage1 = unitContent[0](19, 0);
    EXPECT_NEAR(contentStorage1, 0.005244, 0.000001);
    double contentStorage2 = unitContent[2](19, 0);
    EXPECT_NEAR(contentStorage2, 0.394021, 0.000001);
    EXPECT_NEAR(30.0 - basinOutputs[0].sum() - contentStorage1 - contentStorage2, 0, 0.00000000000001);
}

TEST_F(Solver2LinearStorages, UsingAnalyticLinear) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetSolver("analytic_linear");

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge against the closed-form solution: each storage is
    // exact for a constant inflow over the step, and the second storage receives the
    // first one's average outflow rate (sequential forward substitution).
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble precip = {0.0, 10.0, 10.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                        0.0, 0.0,  0.0,  0.0,  0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double k1 = 0.5f;  // Values stored as float in the parameters
    double k2 = 0.3f;
    double decay1 = std::exp(-k1);
    double decay2 = std::exp(-k2);
    double storage1 = 0.0;
    double storage2 = 0.0;

    ASSERT_EQ(basinOutputs[0].size(), precip.size());
    for (int j = 0; j < basinOutputs[0].size(); ++j) {
        double newStorage1 = storage1 * decay1 + precip[j] / k1 * (1.0 - decay1);
        double outflow1 = precip[j] - (newStorage1 - storage1);
        storage1 = newStorage1;
        double newStorage2 = storage2 * decay2 + outflow1 / k2 * (1.0 - decay2);
        double outflow2 = outflow1 - (newStorage2 - storage2);
        storage2 = newStorage2;
        EXPECT_NEAR(basinOutputs[0][j], outflow2, 0.000000001);
    }

    // The discharge crosses both storages within the first precipitation step (no lag;
    // Euler needs 3 steps to produce the first nonzero discharge on this cascade).
    EXPECT_GT(basinOutputs[0][1], 0.0);

    // Check water balance
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    double contentStorage1 = unitContent[0](19, 0);
    EXPECT_NEAR(contentStorage1, storage1, 0.000000001);
    double contentStorage2 = unitContent[2](19, 0);
    EXPECT_NEAR(contentStorage2, storage2, 0.000000001);
    EXPECT_NEAR(30.0 - basinOutputs[0].sum() - contentStorage1 - contentStorage2, 0, 0.000000001);
}

/**
 * Model: linear storage with max capacity and ET
 */
class SolverLinearStorageWithET : public ::testing::Test {
  protected:
    SettingsModel _model;
    std::unique_ptr<TimeSeriesUniform> _tsPrecip;
    std::unique_ptr<TimeSeriesUniform> _tsPET;

    void SetUp() override {
        _model.SetSolver("euler_explicit");
        _model.SetTimer("2020-01-01", "2020-01-20", 1, "day");

        // Main storage
        _model.AddHydroUnitBrick("storage", "storage");
        _model.AddBrickForcing("precipitation");
        _model.AddBrickLogging("water_content");
        _model.AddBrickParameter("capacity", 20);

        // Linear outflow process
        _model.AddBrickProcess("outflow", "outflow:linear");
        _model.SetProcessParameterValue("response_factor", 0.1f);
        _model.AddProcessLogging("output");
        _model.AddProcessOutput("outlet");

        // ET process
        _model.AddBrickProcess("et", "et:socont");
        _model.AddProcessLogging("output");

        // Overflow process
        _model.AddBrickProcess("overflow", "overflow");
        _model.AddProcessLogging("output");
        _model.AddProcessOutput("outlet");

        _model.AddLoggingToItem("outlet");

        auto dataPrec = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 20), 1,
                                                                TimeUnit::Day);
        dataPrec->SetValues(
            {0.0, 10.0, 10.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        _tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        _tsPrecip->SetData(std::move(dataPrec));

        auto dataPET = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 20), 1,
                                                               TimeUnit::Day);
        dataPET->SetValues(
            {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0});
        _tsPET = std::make_unique<TimeSeriesUniform>(VariableType::PET);
        _tsPET->SetData(std::move(dataPET));
    }
    void TearDown() override {
        // RAII cleanup via unique_ptr
    }
};

TEST_F(SolverLinearStorageWithET, UsingEulerExplicit) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetSolver("euler_explicit");

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPET))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble expectedOutputs = {0.000000, 0.000000, 1.000000, 7.336523, 2.000000, 1.700000, 1.437805,
                                 1.209236, 1.010555, 0.838417, 0.689829, 0.562117, 0.452890, 0.360015,
                                 0.281586, 0.215905, 0.161458, 0.116900, 0.081033, 0.052801};

    for (auto& basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }

    // Check water balance
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    double storageContent = unitContent[0](19, 0);
    EXPECT_NEAR(storageContent, 0.312728, 0.000001);
    EXPECT_NEAR(30.0 - basinOutputs[0].sum() - unitContent[2].sum() - storageContent, 0, 0.00000000000001);
}

TEST_F(SolverLinearStorageWithET, UsingHeunExplicit) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetSolver("heun_explicit");

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPET))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble expectedOutputs = {0.000000, 0.500000, 1.335100, 6.043728, 1.850000, 1.586604, 1.354805,
                                 1.151282, 0.973043, 0.817396, 0.681921, 0.564437, 0.462986, 0.375808,
                                 0.301320, 0.238103, 0.184880, 0.140507, 0.103958, 0.074314};

    for (auto& basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }

    // Check water balance
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    double storageContent = unitContent[0](19, 0);
    EXPECT_NEAR(storageContent, 0.627417, 0.000001);
    EXPECT_NEAR(30.0 - basinOutputs[0].sum() - unitContent[2].sum() - storageContent, 0, 0.00000000000001);
}

/**
 * Convergence order of the solvers on a linear storage.
 *
 * A linear storage (k = 0.5/d) is fed by a constant 5 mm/d precipitation for 2 days,
 * so the run ends mid-transient (k T = 1) where the discretization errors are still
 * evolving (a longer run would end at equilibrium, where all schemes meet the same
 * fixed point and the error ratios saturate). The dynamics are smooth and no
 * constraint binds, so each scheme must converge at its theoretical order: halving
 * the time step divides the error by ~2 (Euler, order 1), ~4 (Heun, order 2) or ~16
 * (RK4, order 4). The analytic solver is exact for this setup at any time step and
 * serves as the reference at the same discretization.
 */
namespace {

double RunLinearStorageAtTimeStep(const string& solverName, int timeStepHours) {
    SettingsModel settings;
    settings.SetSolver(solverName);
    settings.SetTimer("2020-01-01", "2020-01-03", timeStepHours, "hour");

    settings.AddHydroUnitBrick("storage", "storage");
    settings.AddBrickForcing("precipitation");
    settings.AddBrickLogging("water_content");
    settings.AddBrickProcess("outflow", "outflow:linear");
    settings.SetProcessParameterValue("response_factor", 0.5f);
    settings.AddProcessOutput("outlet");
    settings.AddLoggingToItem("outlet");

    int valueCount = 48 / timeStepHours + 1;
    auto data = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 3), timeStepHours,
                                                        TimeUnit::Hour);
    data->SetValues(vecDouble(valueCount, 5.0 * timeStepHours / 24.0));
    auto tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
    tsPrecip->SetData(std::move(data));

    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(settings, basinSettings));
    EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsPrecip))));
    EXPECT_TRUE(model.AttachTimeSeriesToHydroUnits());
    EXPECT_TRUE(model.Run());

    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    return unitContent[0](unitContent[0].rows() - 1, 0);
}

double ConvergenceError(const string& solverName, int timeStepHours) {
    return std::abs(RunLinearStorageAtTimeStep(solverName, timeStepHours) -
                    RunLinearStorageAtTimeStep("analytic_linear", timeStepHours));
}

}  // namespace

TEST(SolverConvergence, EulerExplicitIsFirstOrder) {
    double errorCoarse = ConvergenceError("euler_explicit", 24);
    double errorMid = ConvergenceError("euler_explicit", 12);
    double errorFine = ConvergenceError("euler_explicit", 6);

    EXPECT_GT(errorCoarse, errorMid);
    EXPECT_GT(errorMid, errorFine);
    EXPECT_NEAR(errorCoarse / errorMid, 2.0, 0.4);
    EXPECT_NEAR(errorMid / errorFine, 2.0, 0.4);
}

TEST(SolverConvergence, HeunExplicitIsSecondOrder) {
    double errorCoarse = ConvergenceError("heun_explicit", 24);
    double errorMid = ConvergenceError("heun_explicit", 12);
    double errorFine = ConvergenceError("heun_explicit", 6);

    EXPECT_GT(errorCoarse, errorMid);
    EXPECT_GT(errorMid, errorFine);
    EXPECT_NEAR(errorCoarse / errorMid, 4.0, 0.8);
    EXPECT_NEAR(errorMid / errorFine, 4.0, 0.8);
}

TEST(SolverConvergence, ImplicitEulerIsFirstOrder) {
    double errorCoarse = ConvergenceError("implicit_euler", 24);
    double errorMid = ConvergenceError("implicit_euler", 12);
    double errorFine = ConvergenceError("implicit_euler", 6);

    EXPECT_GT(errorCoarse, errorMid);
    EXPECT_GT(errorMid, errorFine);
    EXPECT_NEAR(errorCoarse / errorMid, 2.0, 0.4);
    EXPECT_NEAR(errorMid / errorFine, 2.0, 0.4);
}

TEST(SolverConvergence, RK4IsFourthOrder) {
    double errorCoarse = ConvergenceError("rk4", 24);
    double errorMid = ConvergenceError("rk4", 12);
    double errorFine = ConvergenceError("rk4", 6);

    EXPECT_GT(errorCoarse, errorMid);
    EXPECT_GT(errorMid, errorFine);
    EXPECT_NEAR(errorCoarse / errorMid, 16.0, 4.0);
    EXPECT_NEAR(errorMid / errorFine, 16.0, 4.0);
}

TEST_F(SolverLinearStorageWithET, UsingImplicitEuler) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetSolver("implicit_euler");

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPET))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Exercises the implicit path with a non-linear process (ET evaluated at the
    // end-of-step content) and the capacity/overflow constraint.
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    // The overflow fires while the storage approaches its 20 mm capacity.
    EXPECT_GT(basinOutputs[0][3], 1.0);

    // Check water balance (mass conservation is exact by construction)
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    double storageContent = unitContent[0](19, 0);
    EXPECT_NEAR(30.0 - basinOutputs[0].sum() - unitContent[2].sum() - storageContent, 0, 0.000000001);
}

TEST_F(SolverLinearStorageWithET, UsingAnalyticLinear) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetSolver("analytic_linear");

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPET))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Exercises the mixed path: linear outflow integrated exactly, ET frozen at its
    // start-of-step rate, capacity excess booked through the overflow process.
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    // The overflow fires while the storage approaches its 20 mm capacity.
    EXPECT_GT(basinOutputs[0][3], 1.0);

    // Check water balance (mass conservation is exact by construction)
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    double storageContent = unitContent[0](19, 0);
    EXPECT_NEAR(30.0 - basinOutputs[0].sum() - unitContent[2].sum() - storageContent, 0, 0.000000001);
}

TEST_F(SolverLinearStorageWithET, UsingRungeKutta) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetSolver("runge_kutta");

    ModelHydro model(&subBasin);
    ASSERT_TRUE(model.Initialize(_model, basinSettings));

    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPrecip))));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(_tsPET))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    // Check resulting discharge
    vecAxd basinOutputs = model.GetLogger()->GetSubBasinValues();

    vecDouble expectedOutputs = {0.000000, 0.467928, 1.312188, 5.989134, 1.856077, 1.591276, 1.358295,
                                 1.153782, 0.974723, 0.818401, 0.682377, 0.564453, 0.462655, 0.375211,
                                 0.300526, 0.237170, 0.183858, 0.139440, 0.102882, 0.073260};

    for (auto& basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            if (j == 3) continue;  // The constraints are handled differently as the Excel computation.
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }

    // Check water balance
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();
    double storageContent = unitContent[0](19, 0);
    EXPECT_NEAR(storageContent, 0.605521, 0.000001);
    EXPECT_NEAR(30.0 - basinOutputs[0].sum() - unitContent[2].sum() - storageContent, 0, 0.00000000000001);
}
