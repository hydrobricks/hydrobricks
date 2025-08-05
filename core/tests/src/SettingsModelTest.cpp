#include <gtest/gtest.h>

#include "SettingsModel.h"

TEST(SettingsModel, SetSharedParameters) {
    SettingsModel settings;

    settings.SetSolver("heun_explicit");
    settings.SetTimer("2020-01-01", "2020-01-08", 1, "day");

    // Precipitation
    settings.GeneratePrecipitationSplitters(true);

    // Land cover bricks
    settings.AddLandCoverBrick("ground", "ground");
    settings.AddLandCoverBrick("glacier_ice", "glacier");
    settings.AddLandCoverBrick("glacier_debris", "glacier");

    // Snowpack brick
    settings.GenerateSnowpacks("melt:temperature_index");

    ASSERT_TRUE(settings.GetHydroUnitBrickSettings("ground_snowpack").processes.size() == 1);
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("ground_snowpack").processes[0].parameters[0]->GetValue(), 3);
    ASSERT_TRUE(settings.GetHydroUnitBrickSettings("glacier_ice_snowpack").processes.size() == 1);
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("glacier_ice_snowpack").processes[0].parameters[0]->GetValue(), 3);
    ASSERT_TRUE(settings.GetHydroUnitBrickSettings("glacier_debris_snowpack").processes.size() == 1);
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("glacier_debris_snowpack").processes[0].parameters[0]->GetValue(), 3);

    // Ice melt processes
    settings.SelectHydroUnitBrick("glacier_ice");
    settings.AddBrickProcess("melt", "melt:temperature_index", "outlet");
    settings.SelectHydroUnitBrick("glacier_debris");
    settings.AddBrickProcess("melt", "melt:temperature_index", "outlet");

    ASSERT_TRUE(settings.GetHydroUnitBrickSettings("glacier_ice").processes.size() == 1);
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("glacier_ice").processes[0].parameters[0]->GetValue(), 3);
    ASSERT_TRUE(settings.GetHydroUnitBrickSettings("glacier_debris").processes.size() == 1);
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("glacier_debris").processes[0].parameters[0]->GetValue(), 3);

    // Change all melt_factor parameters
    EXPECT_TRUE(settings.SetParameterValue("type:snowpack, type:glacier", "melt_factor", 5));

    EXPECT_EQ(settings.GetHydroUnitBrickSettings("ground_snowpack").processes[0].parameters[0]->GetValue(), 5);
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("glacier_ice_snowpack").processes[0].parameters[0]->GetValue(), 5);
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("glacier_debris_snowpack").processes[0].parameters[0]->GetValue(), 5);
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("glacier_ice").processes[0].parameters[0]->GetValue(), 5);
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("glacier_debris").processes[0].parameters[0]->GetValue(), 5);
}
