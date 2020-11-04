
#include <gtest/gtest.h>

#include "helpers.h"
#include "ModelHydro.h"

TEST(ModelHydro, BuildsCorrectly) {
    TimeMachine timer = GenerateTimeMachineDaily();
    auto* subBasin = new SubBasin();
    auto* unit = new HydroUnit();
    subBasin->AddHydroUnit(unit);
    ModelHydro model(subBasin, timer);

    EXPECT_TRUE(model.IsOk());
}

