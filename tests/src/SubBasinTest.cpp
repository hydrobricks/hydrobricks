#include <gtest/gtest.h>

#include "SubBasin.h"

TEST(SubBasin, HasIncomingFlow) {
    SubBasin subBasinIn;
    SubBasin subBasinOut;
    auto* connector = new Connector();
    connector->Connect(&subBasinIn, &subBasinOut);

    EXPECT_TRUE(subBasinOut.HasIncomingFlow());
    EXPECT_FALSE(subBasinIn.HasIncomingFlow());
}

TEST(SubBasin, GetHydroUnitsCount1) {
    SubBasin subBasin;
    auto* unit = new HydroUnit(100);
    subBasin.AddHydroUnit(unit);

    EXPECT_EQ(1, subBasin.GetHydroUnitsCount());
}

TEST(SubBasin, GetHydroUnitsCount3) {
    SubBasin subBasin;
    auto* unit1 = new HydroUnit(100);
    subBasin.AddHydroUnit(unit1);
    auto* unit2 = new HydroUnit(100);
    subBasin.AddHydroUnit(unit2);
    auto* unit3 = new HydroUnit(100);
    subBasin.AddHydroUnit(unit3);

    EXPECT_EQ(3, subBasin.GetHydroUnitsCount());
}

TEST(SubBasin, EmptySubBasinIsNotOk) {
    SubBasin subBasin;

    EXPECT_FALSE(subBasin.IsOk());
}

TEST(SubBasin, SubBasinIsNotOk) {
    SubBasin subBasin;
    auto* unit1 = new HydroUnit(100);
    subBasin.AddHydroUnit(unit1);

    EXPECT_TRUE(subBasin.IsOk());
}