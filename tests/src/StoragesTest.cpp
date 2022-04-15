#include <gtest/gtest.h>

#include "StorageLinear.h"
#include "Parameter.h"
#include "HydroUnit.h"

TEST(StorageLinear, BuildsCorrectly) {
    HydroUnit unit;
    StorageLinear storage(&unit);
    Parameter responseFactor(0.2);
    storage.SetResponseFactor(responseFactor.GetValuePointer());

    EXPECT_FLOAT_EQ(0.2, storage.GetResponseFactor());

    responseFactor.SetValue(0.3);

    EXPECT_FLOAT_EQ(0.3, storage.GetResponseFactor());
}
