#include <gtest/gtest.h>

#include "FluxToBrick.h"
#include "Forcing.h"
#include "Storage.h"
#include "Subbasin.h"
#include "TimeSeriesData.h"
#include "Utils.h"

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

TEST(Subbasin, ForcingIsTheAreaWeightedMeanOfTheHydroUnits) {
    Subbasin subbasin;
    subbasin.AddHydroUnit(std::make_unique<HydroUnit>(100));
    subbasin.AddHydroUnit(std::make_unique<HydroUnit>(300));

    // Created on first use, with a forcing of the same type in every hydro unit.
    Forcing* forcing = subbasin.GetOrCreateForcing(VariableType::PET);
    EXPECT_TRUE(subbasin.HasForcing(VariableType::PET));
    EXPECT_EQ(subbasin.GetOrCreateForcing(VariableType::PET), forcing);
    ASSERT_TRUE(subbasin.GetHydroUnit(0)->HasForcing(VariableType::PET));
    ASSERT_TRUE(subbasin.GetHydroUnit(1)->HasForcing(VariableType::PET));
    EXPECT_FALSE(forcing->IsValid());  // no data attached to the hydro units yet

    TimeSeriesDataRegular data1(GetMJD(2020, 1, 1), GetMJD(2020, 1, 3), 1, TimeUnit::Day);
    data1.SetValues({2.0, 2.0, 2.0});
    TimeSeriesDataRegular data2(GetMJD(2020, 1, 1), GetMJD(2020, 1, 3), 1, TimeUnit::Day);
    data2.SetValues({6.0, 6.0, 6.0});
    subbasin.GetHydroUnit(0)->GetForcing(VariableType::PET)->AttachTimeSeriesData(&data1);
    subbasin.GetHydroUnit(1)->GetForcing(VariableType::PET)->AttachTimeSeriesData(&data2);

    EXPECT_TRUE(forcing->IsValid());
    EXPECT_DOUBLE_EQ(forcing->GetValue(), 0.25 * 2.0 + 0.75 * 6.0);

    // A process may override it for the time step, until the overrides are reset.
    forcing->UpdateValue(1.0);
    EXPECT_DOUBLE_EQ(forcing->GetValue(), 1.0);
    subbasin.ResetForcingUpdates();
    EXPECT_DOUBLE_EQ(forcing->GetValue(), 5.0);
}
