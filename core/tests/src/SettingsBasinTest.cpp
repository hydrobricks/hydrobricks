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

    EXPECT_EQ(unitSettings.surfaceElements.size(), 3);

    EXPECT_TRUE(unitSettings.surfaceElements[0].name.IsSameAs("ground"));
    EXPECT_TRUE(unitSettings.surfaceElements[0].type.IsSameAs("ground"));
    EXPECT_FLOAT_EQ(unitSettings.surfaceElements[0].fraction, 0.80616301f);

    EXPECT_TRUE(unitSettings.surfaceElements[1].name.IsSameAs("glacier-ice"));
    EXPECT_TRUE(unitSettings.surfaceElements[1].type.IsSameAs("glacier"));
    EXPECT_FLOAT_EQ(unitSettings.surfaceElements[1].fraction, 0.08349901f);

    EXPECT_TRUE(unitSettings.surfaceElements[2].name.IsSameAs("glacier-debris"));
    EXPECT_TRUE(unitSettings.surfaceElements[2].type.IsSameAs("glacier"));
    EXPECT_FLOAT_EQ(unitSettings.surfaceElements[2].fraction, 0.11033797f);
}
