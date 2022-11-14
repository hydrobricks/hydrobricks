#include <gtest/gtest.h>

#include "SettingsBasin.h"

TEST(SettingsBasin, ParseNcFile) {
    SettingsBasin settings;
    EXPECT_TRUE(settings.Parse("files/hydro-units-2-glaciers.nc"));

    EXPECT_EQ(settings.GetHydroUnitsNb(), 100);

    HydroUnitSettings unitSettings = settings.GetHydroUnitSettings(83);

    EXPECT_EQ(unitSettings.id, 84);
    EXPECT_EQ(unitSettings.area, 3018000.0);
    EXPECT_EQ(unitSettings.elevation, 4454.0);

    EXPECT_EQ(unitSettings.landCovers.size(), 3);

    EXPECT_TRUE(unitSettings.landCovers[0].name == "ground");
    EXPECT_TRUE(unitSettings.landCovers[0].type == "ground");
    EXPECT_FLOAT_EQ(unitSettings.landCovers[0].fraction, 0.80616301f);

    EXPECT_TRUE(unitSettings.landCovers[1].name == "glacier-ice");
    EXPECT_TRUE(unitSettings.landCovers[1].type == "glacier");
    EXPECT_FLOAT_EQ(unitSettings.landCovers[1].fraction, 0.08349901f);

    EXPECT_TRUE(unitSettings.landCovers[2].name == "glacier-debris");
    EXPECT_TRUE(unitSettings.landCovers[2].type == "glacier");
    EXPECT_FLOAT_EQ(unitSettings.landCovers[2].fraction, 0.11033797f);
}
