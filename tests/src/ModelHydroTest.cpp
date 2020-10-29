
#include <gtest/gtest.h>

#include "helpers.h"
#include "ModelHydro.h"

TEST(ModelHydro, BuildsCorrectly) {
    TimeMachine timer = GenerateTimeMachineDaily();
    SubBasin subBasin;
    HydroUnit unit;
    subBasin.AddHydroUnit(&unit);
    ModelHydro model(&subBasin, timer);

    EXPECT_TRUE(model.IsOk());
}

