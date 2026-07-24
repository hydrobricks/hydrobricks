#include <gtest/gtest.h>

#include "Brick.h"
#include "BrickTypes.h"
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
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("ground_snowpack").processes[0].parameters[0].GetValue(), 3);
    ASSERT_TRUE(settings.GetHydroUnitBrickSettings("glacier_ice_snowpack").processes.size() == 1);
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("glacier_ice_snowpack").processes[0].parameters[0].GetValue(), 3);
    ASSERT_TRUE(settings.GetHydroUnitBrickSettings("glacier_debris_snowpack").processes.size() == 1);
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("glacier_debris_snowpack").processes[0].parameters[0].GetValue(), 3);

    // Ice melt processes
    settings.SelectHydroUnitBrick("glacier_ice");
    settings.AddBrickProcess("melt", "melt:temperature_index", "outlet");
    settings.SelectHydroUnitBrick("glacier_debris");
    settings.AddBrickProcess("melt", "melt:temperature_index", "outlet");

    ASSERT_TRUE(settings.GetHydroUnitBrickSettings("glacier_ice").processes.size() == 1);
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("glacier_ice").processes[0].parameters[0].GetValue(), 3);
    ASSERT_TRUE(settings.GetHydroUnitBrickSettings("glacier_debris").processes.size() == 1);
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("glacier_debris").processes[0].parameters[0].GetValue(), 3);

    // Change all melt_factor parameters
    EXPECT_TRUE(settings.SetParameterValue("type:snowpack, type:glacier", "melt_factor", 5));

    EXPECT_EQ(settings.GetHydroUnitBrickSettings("ground_snowpack").processes[0].parameters[0].GetValue(), 5);
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("glacier_ice_snowpack").processes[0].parameters[0].GetValue(), 5);
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("glacier_debris_snowpack").processes[0].parameters[0].GetValue(), 5);
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("glacier_ice").processes[0].parameters[0].GetValue(), 5);
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("glacier_debris").processes[0].parameters[0].GetValue(), 5);
}

TEST(SettingsModel, LandCoverTaxonomyUsesGenericAndSpecialTypes) {
    // The land-cover refactor removed the behaviourless 'vegetation'/'urban' types;
    // user-defined land covers use the generic type, with special types kept only
    // where there is real behaviour (e.g. glacier).
    EXPECT_EQ(BrickTypeFromString("vegetation"), BrickType::Unknown);
    EXPECT_EQ(BrickTypeFromString("urban"), BrickType::Unknown);
    EXPECT_EQ(BrickTypeFromString("generic_land_cover"), BrickType::GenericLandCover);
    EXPECT_EQ(BrickTypeFromString("glacier"), BrickType::Glacier);

    BrickSettings generic;
    generic.type = "generic_land_cover";
    auto genericBrick = Brick::Factory(generic);
    ASSERT_NE(genericBrick, nullptr);
    EXPECT_EQ(genericBrick->GetCategory(), BrickCategory::GenericLandCover);
    EXPECT_TRUE(genericBrick->IsLandCover());
}

TEST(SettingsModel, SetParameterForAllLandCovers) {
    SettingsModel settings;

    settings.SetSolver("heun_explicit");
    settings.SetTimer("2020-01-01", "2020-01-08", 1, "day");
    settings.GeneratePrecipitationSplitters(true);

    // Mixed land-cover types: generic user-defined covers plus a special (glacier)
    settings.AddLandCoverBrick("ground", "ground");
    settings.AddLandCoverBrick("forest", "generic_land_cover");
    settings.AddLandCoverBrick("glacier", "glacier");

    // Attach a process carrying a parameter to each land cover
    settings.SelectHydroUnitBrick("ground");
    settings.AddBrickProcess("outflow", "outflow:linear", "outlet");
    settings.SelectHydroUnitBrick("forest");
    settings.AddBrickProcess("outflow", "outflow:linear", "outlet");
    settings.SelectHydroUnitBrick("glacier");
    settings.AddBrickProcess("outflow", "outflow:linear", "outlet");

    // 'type:land_cover' targets every land-cover brick at once
    EXPECT_TRUE(settings.SetParameterValue("type:land_cover", "response_factor", 0.3f));

    EXPECT_EQ(settings.GetHydroUnitBrickSettings("ground").processes[0].parameters[0].GetValue(), 0.3f);
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("forest").processes[0].parameters[0].GetValue(), 0.3f);
    EXPECT_EQ(settings.GetHydroUnitBrickSettings("glacier").processes[0].parameters[0].GetValue(), 0.3f);
}
