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
        m_modelSettings.SetTimer("2020-01-01", "2020-01-10", 1, "Day");
        m_modelSettings.AddBrick("linear-storage", "StorageLinear");
        m_modelSettings.AddParameterToCurrentBrick("responseFactor", 0.1f);
        m_modelSettings.AddForcingToCurrentBrick("Precipitation");
        m_modelSettings.AddLoggingToCurrentBrick("content");
        m_modelSettings.AddLoggingToCurrentBrick("output");
        m_modelSettings.AddOutputToCurrentBrick("outlet");
        m_modelSettings.AddLoggingToItem("outlet");

        auto data = new TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                              wxDateTime(20, wxDateTime::Jan, 2020), 1, Day);
        data->SetValues({0.0, 10.0, 10.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
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

    vecDouble expectedOutputs = {0.000000, 1.500000, 3.667500, 5.282288, 4.985304, 3.714052, 2.766968, 2.061392,
                                 1.535737, 1.144124, 0.852372, 0.635017, 0.473088, 0.352450, 0.262576, 0.195619,
                                 0.145736, 0.108573, 0.080887, 0.060261};

    for (int i = 0; i < basinOutputs.size(); ++i) {
        for (int j = 0; j < basinOutputs[i].size(); ++j) {
            EXPECT_EQ(basinOutputs[i][j], expectedOutputs[j]);
        }
    }
}