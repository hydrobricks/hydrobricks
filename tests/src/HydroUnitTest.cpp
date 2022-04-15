#include <gtest/gtest.h>

#include "HydroUnit.h"

TEST(HydroUnit, BuildsCorrectly) {
    HydroUnit unit(100, HydroUnit::Lumped);

    EXPECT_EQ(100, unit.GetArea());
    EXPECT_EQ(HydroUnit::Lumped, unit.GetType());
}

TEST(HydroUnit, HasCorrectId) {
    HydroUnit unit;
    unit.SetId(9);

    EXPECT_EQ(9, unit.GetId());
}