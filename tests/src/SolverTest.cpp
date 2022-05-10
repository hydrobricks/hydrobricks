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
