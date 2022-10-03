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

TEST(SubBasin, GetHydroUnitsCount1) {
    SubBasin subBasin;
    HydroUnit unit(100);
    subBasin.AddHydroUnit(&unit);

    EXPECT_EQ(subBasin.GetHydroUnitsNb(), 1);
}

TEST(SubBasin, GetHydroUnitsCount3) {
    SubBasin subBasin;
    HydroUnit unit1(100);
    subBasin.AddHydroUnit(&unit1);
    HydroUnit unit2(100);
    subBasin.AddHydroUnit(&unit2);
    HydroUnit unit3(100);
    subBasin.AddHydroUnit(&unit3);

    EXPECT_EQ(subBasin.GetHydroUnitsNb(), 3);
}

TEST(SubBasin, EmptySubBasinIsNotOk) {
    wxLogNull logNo;

    SubBasin subBasin;

    EXPECT_FALSE(subBasin.IsOk());
}
/*
TEST(SubBasin, SubBasinIsOk) {
    SubBasin subBasin;
    HydroUnit unit(100);
    subBasin.AddHydroUnit(&unit);

    Storage storage(&unit);
    FluxToBrick inFlux, outFlux;
    storage.AttachFluxIn(&inFlux);
    storage.AttachFluxOut(&outFlux);

    EXPECT_TRUE(subBasin.IsOk());
}*/
