#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

/**
 * Model: simple linear storage
 */
class SolverLinearStorage : public ::testing::Test {
  protected:
    SettingsModel m_model;
    TimeSeriesUniform* m_tsPrecip{};

    virtual void SetUp() {
        m_model.SetSolver("EulerExplicit");
        m_model.SetTimer("2020-01-01", "2020-01-20", 1, "Day");
        m_model.AddBrick("storage", "Storage");
        m_model.AddForcingToCurrentBrick("Precipitation");
        m_model.AddLoggingToCurrentBrick("content");
        m_model.AddProcessToCurrentBrick("outflow", "Outflow:linear");
        m_model.AddParameterToCurrentProcess("responseFactor", 0.3f);
        m_model.AddLoggingToCurrentProcess("output");
        m_model.AddOutputToCurrentProcess("outlet");
        m_model.AddLoggingToItem("outlet");

        auto data = new TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                              wxDateTime(20, wxDateTime::Jan, 2020), 1, Day);
        data->SetValues({0.0, 10.0, 10.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                         0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        m_tsPrecip = new TimeSeriesUniform(Precipitation);
        m_tsPrecip->SetData(data);
    }
    virtual void TearDown() {
        wxDELETE(m_tsPrecip);
    }
};

TEST_F(SolverLinearStorage, UsingEulerExplicit) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    m_model.SetSolver("EulerExplicit");

    ModelHydro model(&subBasin);
    model.Initialize(m_model);

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    vecAxd basinOutputs = model.GetLogger()->GetAggregatedValues();

    vecDouble expectedOutputs = {0.000000, 0.000000, 3.000000, 5.100000, 6.570000, 4.599000, 3.219300, 2.253510,
                                 1.577457, 1.104220, 0.772954, 0.541068, 0.378747, 0.265123, 0.185586, 0.129910,
                                 0.090937, 0.063656, 0.044559, 0.031191};

    for (auto & basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }
}

TEST_F(SolverLinearStorage, UsingHeunExplicit) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    m_model.SetSolver("HeunExplicit");

    ModelHydro model(&subBasin);
    model.Initialize(m_model);

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    vecAxd basinOutputs = model.GetLogger()->GetAggregatedValues();

    vecDouble expectedOutputs = {0.000000, 1.500000, 3.667500, 5.282288, 4.985304, 3.714052, 2.766968, 2.061392,
                                 1.535737, 1.144124, 0.852372, 0.635017, 0.473088, 0.352450, 0.262576, 0.195619,
                                 0.145736, 0.108573, 0.080887, 0.060261};

    for (auto & basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }
}

TEST_F(SolverLinearStorage, UsingRungeKutta) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    m_model.SetSolver("RungeKutta");

    ModelHydro model(&subBasin);
    model.Initialize(m_model);

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    vecAxd basinOutputs = model.GetLogger()->GetAggregatedValues();

    vecDouble expectedOutputs = {0.000000, 1.361250, 3.600090, 5.258707, 5.126222, 3.797698, 2.813477, 2.084329,
                                 1.544149, 1.143964, 0.847491, 0.627853, 0.465137, 0.344591, 0.255286, 0.189125,
                                 0.140111, 0.103800, 0.076899, 0.056969};

    for (auto & basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }
}


/**
 * Model: 2 linear storages in cascade
 */
class Solver2LinearStorages : public ::testing::Test {
  protected:
    SettingsModel m_model;
    TimeSeriesUniform* m_tsPrecip{};

    virtual void SetUp() {
        m_model.SetSolver("EulerExplicit");
        m_model.SetTimer("2020-01-01", "2020-01-20", 1, "Day");
        m_model.AddBrick("storage-1", "Storage");
        m_model.AddForcingToCurrentBrick("Precipitation");
        m_model.AddLoggingToCurrentBrick("content");
        m_model.AddProcessToCurrentBrick("outflow", "Outflow:linear");
        m_model.AddParameterToCurrentProcess("responseFactor", 0.5f);
        m_model.AddLoggingToCurrentProcess("output");
        m_model.AddOutputToCurrentProcess("storage-2");
        m_model.AddBrick("storage-2", "Storage");
        m_model.AddLoggingToCurrentBrick("content");
        m_model.AddProcessToCurrentBrick("outflow", "Outflow:linear");
        m_model.AddParameterToCurrentProcess("responseFactor", 0.3f);
        m_model.AddLoggingToCurrentProcess("output");
        m_model.AddOutputToCurrentProcess("outlet");
        m_model.AddLoggingToItem("outlet");

        auto data = new TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                              wxDateTime(20, wxDateTime::Jan, 2020), 1, Day);
        data->SetValues({0.0, 10.0, 10.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                         0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        m_tsPrecip = new TimeSeriesUniform(Precipitation);
        m_tsPrecip->SetData(data);
    }
    virtual void TearDown() {
        wxDELETE(m_tsPrecip);
    }
};

TEST_F(Solver2LinearStorages, UsingEulerExplicit) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    m_model.SetSolver("EulerExplicit");

    ModelHydro model(&subBasin);
    model.Initialize(m_model);

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    vecAxd basinOutputs = model.GetLogger()->GetAggregatedValues();

    vecDouble expectedOutputs = {0.000000, 0.000000, 0.000000, 1.500000, 3.300000, 4.935000, 4.767000, 3.993150,
                                 3.123330, 2.350394, 1.727307, 1.250130, 0.895599, 0.637173, 0.451148, 0.318367,
                                 0.224139, 0.157538, 0.110597, 0.077578};

    for (auto & basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }
}

TEST_F(Solver2LinearStorages, UsingHeunExplicit) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    m_model.SetSolver("HeunExplicit");

    ModelHydro model(&subBasin);
    model.Initialize(m_model);

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    vecAxd basinOutputs = model.GetLogger()->GetAggregatedValues();

    vecDouble expectedOutputs = {0.000000, 0.000000, 1.200000, 2.600250, 3.959843, 3.970493, 3.595773, 3.077449,
                                 2.541823, 2.049360, 1.624087, 1.270766, 0.984734, 0.757385, 0.579101, 0.440711,
                                 0.334130, 0.252552, 0.190417, 0.143277};

    for (auto & basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }
}

TEST_F(Solver2LinearStorages, UsingRungeKutta) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    m_model.SetSolver("RungeKutta");

    ModelHydro model(&subBasin);
    model.Initialize(m_model);

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    vecAxd basinOutputs = model.GetLogger()->GetAggregatedValues();

    vecDouble expectedOutputs = {0.000000, 0.200000, 1.158225, 2.490032, 3.654047, 3.935308, 3.660692, 3.164185,
                                 2.618533, 2.106397, 1.661518, 1.292212, 0.994512, 0.759339, 0.576240, 0.435209,
                                 0.327461, 0.245654, 0.183846, 0.137326};

    for (auto & basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }
}

/**
 * Model: linear storage with max capacity and ET
 */
class SolverLinearStorageWithET : public ::testing::Test {
  protected:
    SettingsModel m_model;
    TimeSeriesUniform* m_tsPrecip{};
    TimeSeriesUniform* m_tsPET{};

    virtual void SetUp() {
        m_model.SetSolver("EulerExplicit");
        m_model.SetTimer("2020-01-01", "2020-01-20", 1, "Day");
        m_model.AddBrick("storage", "Storage");
        m_model.AddForcingToCurrentBrick("Precipitation");
        m_model.AddLoggingToCurrentBrick("content");
        m_model.AddParameterToCurrentBrick("capacity", 15);
        m_model.AddProcessToCurrentBrick("outflow", "Outflow:linear");
        m_model.AddParameterToCurrentProcess("responseFactor", 0.2f);
        m_model.AddLoggingToCurrentProcess("output");
        m_model.AddOutputToCurrentProcess("outlet");
        m_model.AddProcessToCurrentBrick("ET", "ET:Socont");
        m_model.AddForcingToCurrentProcess("PET");
        m_model.AddLoggingToItem("outlet");

        auto dataPrec = new TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                                  wxDateTime(20, wxDateTime::Jan, 2020), 1, Day);
        dataPrec->SetValues({0.0, 10.0, 10.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                             0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        m_tsPrecip = new TimeSeriesUniform(Precipitation);
        m_tsPrecip->SetData(dataPrec);

        auto dataPET = new TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                                 wxDateTime(20, wxDateTime::Jan, 2020), 1, Day);
        dataPET->SetValues({2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0,
                            2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0});
        m_tsPET = new TimeSeriesUniform(PET);
        m_tsPET->SetData(dataPET);
    }
    virtual void TearDown() {
        wxDELETE(m_tsPrecip);
        wxDELETE(m_tsPET);
    }
};

TEST_F(SolverLinearStorageWithET, UsingEulerExplicit) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    m_model.SetSolver("EulerExplicit");

    ModelHydro model(&subBasin);
    model.Initialize(m_model);

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPET));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    vecAxd basinOutputs = model.GetLogger()->GetAggregatedValues();

    vecDouble expectedOutputs = {0.000000, 0.000000, 3.367007, 8.000000, 3.000000, 2.000000, 1.273401, 0.758117,
                                 0.405414, 0.177287, 0.044591, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000,
                                 0.000000, 0.000000, 0.000000, 0.000000};

    for (auto & basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }
}

TEST_F(SolverLinearStorageWithET, UsingHeunExplicit) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    m_model.SetSolver("HeunExplicit");

    ModelHydro model(&subBasin);
    model.Initialize(m_model);

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPET));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    vecAxd basinOutputs = model.GetLogger()->GetAggregatedValues();

    vecDouble expectedOutputs = {0.000000, 1.000000, 2.318350, 7.156080, 2.500000, 1.754243, 1.193078, 0.778497,
                                 0.479776, 0.272117, 0.135535, 0.053956, 0.024801, 0.009920, 0.003968, 0.001587,
                                 0.000635, 0.000254, 0.000102, 0.000041};

    for (auto & basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }
}

TEST_F(SolverLinearStorageWithET, UsingRungeKutta) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    m_model.SetSolver("RungeKutta");

    ModelHydro model(&subBasin);
    model.Initialize(m_model);

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AddTimeSeries(m_tsPET));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    vecAxd basinOutputs = model.GetLogger()->GetAggregatedValues();

    vecDouble expectedOutputs = {0.000000, 0.867934, 2.260862, 7.068382, 2.541888, 1.776578, 1.201974, 0.778559,
                                 0.474453, 0.263977, 0.126492, 0.046199, 0.014299, 0.004553, 0.001450, 0.000462,
                                 0.000147, 0.000047, 0.000015, 0.000005};

    for (auto & basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }
}
