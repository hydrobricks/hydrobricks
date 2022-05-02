#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

class ModelHydroSingleLinearStorage : public ::testing::Test {
  protected:
    SettingsModel m_modelSettings;
    TimeSeriesUniform* m_tsPrecipSingleRainyDay;

    virtual void SetUp() {
        m_modelSettings.SetSolver("ExplicitEuler");
        m_modelSettings.SetTimer("2020-01-01", "2020-01-10", 1, "Day");
        m_modelSettings.AddBrick("linear-storage", "StorageLinear");
        m_modelSettings.AddParameterToCurrentBrick("responseFactor", 0.1f);
        m_modelSettings.AddForcingToCurrentBrick("Precipitation");
        m_modelSettings.AddLoggingToCurrentBrick("linear-storage-content");
        m_modelSettings.AddOutputToCurrentBrick("outlet");
        m_modelSettings.AddLoggingToItem("outlet");

        auto data = new TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                              wxDateTime(10, wxDateTime::Jan, 2020), 1, Day);
        data->SetValues({0.0, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        m_tsPrecipSingleRainyDay = new TimeSeriesUniform(Precipitation);
        m_tsPrecipSingleRainyDay->SetData(data);
    }
    virtual void TearDown() {
        wxDELETE(m_tsPrecipSingleRainyDay);
    }
};

TEST_F(ModelHydroSingleLinearStorage, BuildsCorrectly) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    ModelHydro model(&subBasin);
    model.Initialize(m_modelSettings);

    EXPECT_TRUE(model.IsOk());
}

TEST_F(ModelHydroSingleLinearStorage, RunsCorrectly) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    ModelHydro model(&subBasin);
    model.Initialize(m_modelSettings);

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecipSingleRainyDay));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
}
