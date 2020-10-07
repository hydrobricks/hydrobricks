
#include <gtest/gtest.h>

#include "SubCatchment.h"

TEST(SubCatchment, HasIncomingFlow) {
    SubCatchment subCatchmentIn;
    SubCatchment subCatchmentOut;
    Connector connector;
    connector.Connect(&subCatchmentIn, &subCatchmentOut);

    EXPECT_TRUE(subCatchmentOut.HasIncomingFlow());
    EXPECT_FALSE(subCatchmentIn.HasIncomingFlow());
}

TEST(SubCatchment, GetHydroUnitsCount1) {
    SubCatchment subCatchment;
    HydroUnit unit(100);
    subCatchment.AddHydroUnit(&unit);

    EXPECT_EQ(1, subCatchment.GetHydroUnitsCount());
}

TEST(SubCatchment, GetHydroUnitsCount3) {
    SubCatchment subCatchment;
    HydroUnit unit1(100);
    subCatchment.AddHydroUnit(&unit1);
    HydroUnit unit2(100);
    subCatchment.AddHydroUnit(&unit2);
    HydroUnit unit3(100);
    subCatchment.AddHydroUnit(&unit3);

    EXPECT_EQ(3, subCatchment.GetHydroUnitsCount());
}