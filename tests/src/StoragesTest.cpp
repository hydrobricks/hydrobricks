#include <gtest/gtest.h>

#include "StorageLinear.h"
#include "Parameter.h"
#include "FluxDirect.h"
#include "HydroUnit.h"

TEST(StorageLinear, BuildsCorrectly) {
    HydroUnit unit;
    StorageLinear storage(&unit);
    FluxDirect inFlux, outFlux;
    storage.AttachFluxIn(&inFlux);
    storage.AttachFluxOut(&outFlux);

    EXPECT_TRUE(storage.IsOk());
}

TEST(StorageLinear, MissingHydroUnit) {
    StorageLinear storage(nullptr);
    FluxDirect inFlux, outFlux;
    storage.AttachFluxIn(&inFlux);
    storage.AttachFluxOut(&outFlux);

    EXPECT_FALSE(storage.IsOk());
}

TEST(StorageLinear, MissingInFlux) {
    HydroUnit unit;
    StorageLinear storage(&unit);
    Parameter responseFactor(0.2f);
    storage.SetResponseFactor(responseFactor.GetValuePointer());
    FluxDirect outFlux;
    storage.AttachFluxOut(&outFlux);

    EXPECT_FALSE(storage.IsOk());
}

TEST(StorageLinear, MissingOutFlux) {
    HydroUnit unit;
    StorageLinear storage(&unit);
    Parameter responseFactor(0.2f);
    storage.SetResponseFactor(responseFactor.GetValuePointer());
    FluxDirect inFlux;
    storage.AttachFluxIn(&inFlux);

    EXPECT_FALSE(storage.IsOk());
}

TEST(StorageLinear, CorrectlyLinksToParameters) {
    HydroUnit unit;
    StorageLinear storage(&unit);
    Parameter responseFactor(0.2f);
    storage.SetResponseFactor(responseFactor.GetValuePointer());

    EXPECT_FLOAT_EQ(0.2f, storage.GetResponseFactor());

    responseFactor.SetValue(0.3f);

    EXPECT_FLOAT_EQ(0.3f, storage.GetResponseFactor());
}

TEST(StorageLinear, ComputesCorrectly) {
    HydroUnit unit;
    StorageLinear storage(&unit);
    Parameter responseFactor(0.2f);
    storage.SetResponseFactor(responseFactor.GetValuePointer());
    FluxDirect inFlux, outFlux;
    storage.AttachFluxIn(&inFlux);
    storage.AttachFluxOut(&outFlux);

    EXPECT_FLOAT_EQ(0.2f, storage.GetResponseFactor());

    responseFactor.SetValue(0.3);

    EXPECT_FLOAT_EQ(0.3f, storage.GetResponseFactor());
}