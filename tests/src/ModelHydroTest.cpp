#include <gtest/gtest.h>

#include "helpers.h"
#include "ModelHydro.h"
#include "SolverRungeKuttaMethod.h"
#include "StorageLinear.h"
#include "FluxDirect.h"

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
