#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

TEST(Solver, FactoryBuildsSolvers) {
    SolverSettings settings;
    Solver* solver;

    settings.name = "rk4";
    solver = Solver::Factory(settings);
    EXPECT_TRUE(solver != nullptr);
    wxDELETE(solver);

    settings.name = "runge_kutta";
    solver = Solver::Factory(settings);
    EXPECT_TRUE(solver != nullptr);
    wxDELETE(solver);

    settings.name = "runge_kutta";
    solver = Solver::Factory(settings);
    EXPECT_TRUE(solver != nullptr);
    wxDELETE(solver);

    settings.name = "euler_explicit";
    solver = Solver::Factory(settings);
    EXPECT_TRUE(solver != nullptr);
    wxDELETE(solver);

    settings.name = "heun_explicit";
    solver = Solver::Factory(settings);
    EXPECT_TRUE(solver != nullptr);
    wxDELETE(solver);
}

TEST(Solver, FactoryThrowsExceptionIfNameInvalid) {
    SolverSettings settings;

    settings.name = "invalid_name";
    EXPECT_THROW(Solver::Factory(settings), InvalidArgument);
}

/**
 * Model: simple linear storage
 */
class SolverLinearStorage : public ::testing::Test {
  protected:
    SettingsModel _model;
    TimeSeriesUniform* _tsPrecip{};

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

        auto data = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 20), 1, Day);
        data->SetValues(
            {0.0, 10.0, 10.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        _tsPrecip = new TimeSeriesUniform(Precipitation);
        _tsPrecip->SetData(data);
    }
    void TearDown() override {
        wxDELETE(_tsPrecip);
    }
};

TEST_F(SolverLinearStorage, UsingEulerExplicit) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetSolver("euler_explicit");

    ModelHydro model(&subBasin);
    model.Initialize(_model, basinSettings);

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
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
    model.Initialize(_model, basinSettings);

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
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
    model.Initialize(_model, basinSettings);

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
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

/**
 * Model: 2 linear storages in cascade
 */
class Solver2LinearStorages : public ::testing::Test {
  protected:
    SettingsModel _model;
    TimeSeriesUniform* _tsPrecip{};

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

        auto data = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 20), 1, Day);
        data->SetValues(
            {0.0, 10.0, 10.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        _tsPrecip = new TimeSeriesUniform(Precipitation);
        _tsPrecip->SetData(data);
    }
    void TearDown() override {
        wxDELETE(_tsPrecip);
    }
};

TEST_F(Solver2LinearStorages, UsingEulerExplicit) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetSolver("euler_explicit");

    ModelHydro model(&subBasin);
    model.Initialize(_model, basinSettings);

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
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
    model.Initialize(_model, basinSettings);

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
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
    model.Initialize(_model, basinSettings);

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
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

/**
 * Model: linear storage with max capacity and ET
 */
class SolverLinearStorageWithET : public ::testing::Test {
  protected:
    SettingsModel _model;
    TimeSeriesUniform* _tsPrecip{};
    TimeSeriesUniform* _tsPET{};

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

        auto dataPrec = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 20), 1, Day);
        dataPrec->SetValues(
            {0.0, 10.0, 10.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        _tsPrecip = new TimeSeriesUniform(Precipitation);
        _tsPrecip->SetData(dataPrec);

        auto dataPET = new TimeSeriesDataRegular(GetMJD(2020, 1, 1), GetMJD(2020, 1, 20), 1, Day);
        dataPET->SetValues(
            {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0});
        _tsPET = new TimeSeriesUniform(PET);
        _tsPET->SetData(dataPET);
    }
    void TearDown() override {
        wxDELETE(_tsPrecip);
        wxDELETE(_tsPET);
    }
};

TEST_F(SolverLinearStorageWithET, UsingEulerExplicit) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetSolver("euler_explicit");

    ModelHydro model(&subBasin);
    model.Initialize(_model, basinSettings);

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(_tsPET));
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
    model.Initialize(_model, basinSettings);

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(_tsPET));
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

TEST_F(SolverLinearStorageWithET, UsingRungeKutta) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    _model.SetSolver("runge_kutta");

    ModelHydro model(&subBasin);
    model.Initialize(_model, basinSettings);

    ASSERT_TRUE(model.AddTimeSeries(_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(_tsPET));
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
