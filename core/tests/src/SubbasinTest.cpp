#include <gtest/gtest.h>

#include "FluxToBrick.h"
#include "Storage.h"
#include "Subbasin.h"

TEST(Subbasin, HasIncomingFlow) {
    Subbasin subbasinIn;
    Subbasin subbasinOut;
    Connector connector;
    connector.Connect(&subbasinIn, &subbasinOut);

    EXPECT_TRUE(subbasinOut.HasIncomingFlow());
    EXPECT_FALSE(subbasinIn.HasIncomingFlow());
}

TEST(Subbasin, GetHydroUnitCount1) {
    Subbasin subbasin;
    subbasin.AddHydroUnit(std::make_unique<HydroUnit>(100));

    EXPECT_EQ(subbasin.GetHydroUnitCount(), 1);
}

TEST(Subbasin, GetHydroUnitCount3) {
    Subbasin subbasin;
    subbasin.AddHydroUnit(std::make_unique<HydroUnit>(100));
    subbasin.AddHydroUnit(std::make_unique<HydroUnit>(100));
    subbasin.AddHydroUnit(std::make_unique<HydroUnit>(100));

    EXPECT_EQ(subbasin.GetHydroUnitCount(), 3);
}

TEST(Subbasin, EmptySubbasinIsNotOk) {
    Subbasin subbasin;

    EXPECT_FALSE(subbasin.IsValid());
}

TEST(Subbasin, SubbasinIsOk) {
    Subbasin subbasin;
    subbasin.AddHydroUnit(std::make_unique<HydroUnit>(100));

    EXPECT_TRUE(subbasin.IsValid());
}
