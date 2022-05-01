#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "ParameterSet.h"
#include "TimeSeriesUniform.h"

class ModelHydroSingleLinearStorage : public ::testing::Test {
  protected:
    ParameterSet m_parameterSet;
    TimeSeriesUniform* m_tsPrecipSingleRainyDay;

    virtual void SetUp() {
        m_parameterSet.SetSolver("ExplicitEuler");
        m_parameterSet.SetTimer("2020-01-01", "2020-01-10", 1, "Day");
        m_parameterSet.AddBrick("linear-storage", "StorageLinear");
        m_parameterSet.AddParameterToCurrentBrick("responseFactor", 0.1f);
        m_parameterSet.AddForcingToCurrentBrick("Precipitation");
        m_parameterSet.AddOutputToCurrentBrick("outlet");

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
    model.Initialize(m_parameterSet);

    EXPECT_TRUE(model.IsOk());
}

TEST_F(ModelHydroSingleLinearStorage, RunsCorrectly) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    ModelHydro model(&subBasin);
    model.Initialize(m_parameterSet);

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecipSingleRainyDay));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
}
