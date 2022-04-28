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


    EXPECT_FALSE(model->Run());

    wxDELETE(model);
}


/*
TEST(ModelHydro, BuildsCorrectly) {
    SolverRungeKuttaMethod solver;
    Processor processor(&solver);
    TimeMachine timer = GenerateTimeMachineDaily();
    SubBasin subBasin;
    HydroUnit unit;

    StorageLinear storage(&unit);
    FluxDirect inFlux, outFlux;
    storage.AttachFluxIn(&inFlux);
    storage.AttachFluxOut(&outFlux);

    unit.SetInputFlux(&inFlux);

    subBasin.AddHydroUnit(&unit);
    ModelHydro model(&processor, &subBasin, &timer);

    EXPECT_TRUE(model.IsOk());
}
*/
/*
TEST(ModelHydro, RunsCorrectly) {
    SolverRK4 solver;
    Processor processor(&solver);
    TimeMachine timer = GenerateTimeMachineDaily();
    SubBasin subBasin;
    HydroUnit unit;

    Forcing precipitation(Precipitation);
    unit.AddForcing(&precipitation);
    FluxDirect precipFlux;
    ForcingFluxConnector connector(&precipitation, &precipFlux);

    StorageLinear storage(&unit);
    storage.AttachFluxIn(&precipFlux);
    FluxDirect outFlux;
    storage.AttachFluxOut(&outFlux);


    subBasin.AddHydroUnit(&unit);
    ModelHydro model(&processor, &subBasin, &timer);

    EXPECT_FALSE(model.Run());
}
*/
