#include <gtest/gtest.h>

#include "HydroUnit.h"

TEST(HydroUnit, BuildsCorrectly) {
    HydroUnit unit(100, HydroUnit::Lumped);

    EXPECT_EQ(unit.GetArea(), 100);
    EXPECT_EQ(unit.GetType(), HydroUnit::Lumped);
}

TEST(HydroUnit, HasCorrectId) {
    HydroUnit unit(100);
    unit.SetId(9);

    EXPECT_EQ(unit.GetId(), 9);
}
