#include <gtest/gtest.h>

#include "FluxToBrick.h"
#include "Storage.h"
#include "SubBasin.h"

TEST(SubBasin, HasIncomingFlow) {
    SubBasin subBasinIn;
    SubBasin subBasinOut;
    Connector connector;
    connector.Connect(&subBasinIn, &subBasinOut);

    EXPECT_TRUE(subBasinOut.HasIncomingFlow());
    EXPECT_FALSE(subBasinIn.HasIncomingFlow());
}

TEST(SubBasin, GetHydroUnitCount1) {
    SubBasin subBasin;
    subBasin.AddHydroUnit(std::make_unique<HydroUnit>(100));

    EXPECT_EQ(subBasin.GetHydroUnitCount(), 1);
}

TEST(SubBasin, GetHydroUnitCount3) {
    SubBasin subBasin;
    subBasin.AddHydroUnit(std::make_unique<HydroUnit>(100));
    subBasin.AddHydroUnit(std::make_unique<HydroUnit>(100));
    subBasin.AddHydroUnit(std::make_unique<HydroUnit>(100));

    EXPECT_EQ(subBasin.GetHydroUnitCount(), 3);
}

TEST(SubBasin, EmptySubBasinIsNotOk) {
    wxLogNull logNo;

    SubBasin subBasin;

    EXPECT_FALSE(subBasin.IsValid());
}

TEST(SubBasin, SubBasinIsOk) {
    SubBasin subBasin;
    subBasin.AddHydroUnit(std::make_unique<HydroUnit>(100));

    EXPECT_TRUE(subBasin.IsValid());
}
