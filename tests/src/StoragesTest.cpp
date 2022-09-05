#include <gtest/gtest.h>

#include "FluxToBrick.h"
#include "HydroUnit.h"
#include "Parameter.h"
#include "Storage.h"
/*
TEST(StorageLinear, BuildsCorrectly) {
    HydroUnit unit;
    Storage storage(&unit);
    FluxToBrick inFlux, outFlux;
    storage.AttachFluxIn(&inFlux);
    storage.AttachFluxOut(&outFlux);

    EXPECT_TRUE(storage.IsOk());
}

TEST(StorageLinear, MissingHydroUnit) {
    wxLogNull logNo;

    Storage storage(nullptr);
    FluxToBrick inFlux, outFlux;
    storage.AttachFluxIn(&inFlux);
    storage.AttachFluxOut(&outFlux);

    EXPECT_FALSE(storage.IsOk());
}

TEST(StorageLinear, MissingInFlux) {
    wxLogNull logNo;

    HydroUnit unit;
    StorageLinear storage(&unit);
    Parameter responseFactor("responseFactor", 0.2f);
    storage.SetResponseFactor(responseFactor.GetValuePointer());
    FluxToBrick outFlux;
    storage.AttachFluxOut(&outFlux);

    EXPECT_FALSE(storage.IsOk());
}

TEST(StorageLinear, MissingOutFlux) {
    wxLogNull logNo;

    HydroUnit unit;
    StorageLinear storage(&unit);
    Parameter responseFactor("responseFactor", 0.2f);
    storage.SetResponseFactor(responseFactor.GetValuePointer());
    FluxToBrick inFlux;
    storage.AttachFluxIn(&inFlux);

    EXPECT_FALSE(storage.IsOk());
}

TEST(StorageLinear, CorrectlyLinksToParameters) {
    HydroUnit unit;
    StorageLinear storage(&unit);
    Parameter responseFactor("responseFactor", 0.2f);
    storage.SetResponseFactor(responseFactor.GetValuePointer());

    EXPECT_FLOAT_EQ(0.2f, storage.GetResponseFactor());

    responseFactor.SetValue(0.3f);

    EXPECT_FLOAT_EQ(0.3f, storage.GetResponseFactor());
}

TEST(StorageLinear, ComputesCorrectly) {
    HydroUnit unit;
    StorageLinear storage(&unit);
    Parameter responseFactor("responseFactor", 0.2f);
    storage.SetResponseFactor(responseFactor.GetValuePointer());
    FluxToBrick inFlux, outFlux;
    storage.AttachFluxIn(&inFlux);
    storage.AttachFluxOut(&outFlux);

    EXPECT_FLOAT_EQ(0.2f, storage.GetResponseFactor());

    responseFactor.SetValue(0.3f);

    EXPECT_FLOAT_EQ(0.3f, storage.GetResponseFactor());
}
*/