#include <gtest/gtest.h>

#include "SubBasin.h"
#include "StorageLinear.h"
#include "FluxDirect.h"

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

    EXPECT_EQ(1, subBasin.GetHydroUnitsNb());
}

TEST(SubBasin, GetHydroUnitsCount3) {
    SubBasin subBasin;
    HydroUnit unit1(100);
    subBasin.AddHydroUnit(&unit1);
    HydroUnit unit2(100);
    subBasin.AddHydroUnit(&unit2);
    HydroUnit unit3(100);
    subBasin.AddHydroUnit(&unit3);

    EXPECT_EQ(3, subBasin.GetHydroUnitsNb());
}

TEST(SubBasin, EmptySubBasinIsNotOk) {
    wxLogNull logNo;
    
    SubBasin subBasin;

    EXPECT_FALSE(subBasin.IsOk());
}

TEST(SubBasin, SubBasinIsOk) {
    SubBasin subBasin;
    HydroUnit unit(100);
    subBasin.AddHydroUnit(&unit);

    StorageLinear storage(&unit);
    FluxDirect inFlux, outFlux;
    storage.AttachFluxIn(&inFlux);
    storage.AttachFluxOut(&outFlux);

    EXPECT_TRUE(subBasin.IsOk());
}