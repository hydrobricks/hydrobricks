#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "ParameterSet.h"
#include "helpers.h"

TEST(ModelHydro, BuildsCorrectly) {
    ParameterSet parameterSet;
    parameterSet.SetSolver("ExplicitEuler");
    parameterSet.SetTimer("2020-01-01", "2020-01-10", 1, "Day");
    parameterSet.AddBrick("linear-storage", "StorageLinear");
    parameterSet.AddParameterToCurrentBrick("responseFactor", 0.1);
    parameterSet.AddForcingToCurrentBrick("Precipitation");
    parameterSet.AddOutputToCurrentBrick("outlet");

    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    ModelHydro* model = ModelHydro::Factory(parameterSet, &subBasin);

    EXPECT_TRUE(model->IsOk());

    wxDELETE(model);
}

TEST(ModelHydro, RunsCorrectly) {
    ParameterSet parameterSet;
    parameterSet.SetSolver("ExplicitEuler");
    parameterSet.SetTimer("2020-01-01", "2020-01-10", 1, "Day");
    parameterSet.AddBrick("linear-storage", "StorageLinear");
    parameterSet.AddParameterToCurrentBrick("responseFactor", 0.1);
    parameterSet.AddForcingToCurrentBrick("Precipitation");
    parameterSet.AddOutputToCurrentBrick("outlet");

    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);

    ModelHydro* model = ModelHydro::Factory(parameterSet, &subBasin);

    EXPECT_FALSE(model->Run());

    wxDELETE(model);
}
