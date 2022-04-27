#include <gtest/gtest.h>

#include "FluxDirect.h"
#include "ForcingFluxConnector.h"
#include "ModelHydro.h"
#include "SolverRK4.h"
#include "StorageLinear.h"
#include "helpers.h"
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
