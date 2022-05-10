#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

class SingleLinearStorage : public ::testing::Test {
  protected:
    SettingsModel m_modelSettings;
    TimeSeriesUniform* m_tsPrecipSingleRainyDay{};

    virtual void SetUp() {
        m_modelSettings.SetSolver("EulerExplicit");
        m_modelSettings.SetTimer("2020-01-01", "2020-01-20", 1, "Day");
        m_modelSettings.AddBrick("storage", "Storage");
        m_modelSettings.AddForcingToCurrentBrick("Precipitation");
        m_modelSettings.AddLoggingToCurrentBrick("content");
        m_modelSettings.AddProcessToCurrentBrick("outflow", "Outflow:linear");
        m_modelSettings.AddParameterToCurrentProcess("responseFactor", 0.3f);
        m_modelSettings.AddLoggingToCurrentProcess("output");
        m_modelSettings.AddOutputToCurrentProcess("outlet");
        m_modelSettings.AddLoggingToItem("outlet");

        auto data = new TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                              wxDateTime(20, wxDateTime::Jan, 2020), 1, Day);
        data->SetValues({0.0, 10.0, 10.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                         0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        m_tsPrecipSingleRainyDay = new TimeSeriesUniform(Precipitation);
        m_tsPrecipSingleRainyDay->SetData(data);
    }
    virtual void TearDown() {
        wxDELETE(m_tsPrecipSingleRainyDay);
    }
};

TEST_F(SingleLinearStorage, UsingEulerExplicit) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    m_modelSettings.SetSolver("EulerExplicit");

    ModelHydro model(&subBasin);
    model.Initialize(m_modelSettings);

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecipSingleRainyDay));
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

TEST_F(SingleLinearStorage, UsingHeunExplicit) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    m_modelSettings.SetSolver("HeunExplicit");

    ModelHydro model(&subBasin);
    model.Initialize(m_modelSettings);

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecipSingleRainyDay));
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

TEST_F(SingleLinearStorage, UsingRungeKutta) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    m_modelSettings.SetSolver("RungeKutta");

    ModelHydro model(&subBasin);
    model.Initialize(m_modelSettings);

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecipSingleRainyDay));
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