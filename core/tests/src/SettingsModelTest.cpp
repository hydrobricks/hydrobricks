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

TEST(SettingsModel, DeprecatedLandCoverSynonymsMapToGeneric) {
    // 'vegetation' and 'urban' are deprecated synonyms that now build plain generic
    // land covers (they carried no special behaviour).
    EXPECT_EQ(BrickTypeFromString("vegetation"), BrickType::GenericLandCover);
    EXPECT_EQ(BrickTypeFromString("urban"), BrickType::GenericLandCover);

    BrickSettings vegetation;
    vegetation.type = "vegetation";
    auto vegetationBrick = Brick::Factory(vegetation);
    ASSERT_NE(vegetationBrick, nullptr);
    EXPECT_EQ(vegetationBrick->GetCategory(), BrickCategory::GenericLandCover);
    EXPECT_TRUE(vegetationBrick->IsLandCover());
}

TEST(SettingsModel, SetParameterForAllLandCovers) {
    SettingsModel settings;

    settings.SetSolver("heun_explicit");
    settings.SetTimer("2020-01-01", "2020-01-08", 1, "day");
    settings.GeneratePrecipitationSplitters(true);

    // Mixed land-cover types, including a deprecated synonym (now generic)
    settings.AddLandCoverBrick("ground", "ground");
    settings.AddLandCoverBrick("forest", "vegetation");
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
