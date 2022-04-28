#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "ParameterSet.h"

class ModelHydroSingleLinearStorage : public ::testing::Test {
  protected:
    ParameterSet m_parameterSet;

    virtual void SetUp() {
        m_parameterSet.SetSolver("ExplicitEuler");
        m_parameterSet.SetTimer("2020-01-01", "2020-01-10", 1, "Day");
        m_parameterSet.AddBrick("linear-storage", "StorageLinear");
        m_parameterSet.AddParameterToCurrentBrick("responseFactor", 0.1);
        m_parameterSet.AddForcingToCurrentBrick("Precipitation");
        m_parameterSet.AddOutputToCurrentBrick("outlet");
    }
    virtual void TearDown() {
    }
};

TEST_F(ModelHydroSingleLinearStorage, BuildsCorrectly) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    ModelHydro* model = ModelHydro::Factory(m_parameterSet, &subBasin);

    EXPECT_TRUE(model->IsOk());

    wxDELETE(model);
}

TEST_F(ModelHydroSingleLinearStorage, RunsCorrectly) {
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    ModelHydro* model = ModelHydro::Factory(m_parameterSet, &subBasin);

    EXPECT_FALSE(model->Run());

    wxDELETE(model);
}
