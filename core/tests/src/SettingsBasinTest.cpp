#include <gtest/gtest.h>

#include <filesystem>

#include "FileNetcdf.h"
#include "SettingsBasin.h"

TEST(SettingsBasin, ParseNcFile) {
    SettingsBasin settings;
    EXPECT_TRUE(settings.Parse("files/hydro-units-2-glaciers.nc"));

    EXPECT_EQ(settings.GetHydroUnitCount(), 100);

    HydroUnitSettings unitSettings = settings.GetHydroUnitSettings(83);

    EXPECT_EQ(unitSettings.id, 84);
    EXPECT_EQ(unitSettings.area, 3018000.0);

    EXPECT_GE(unitSettings.propertiesDouble.size(), 1);
    EXPECT_TRUE(unitSettings.propertiesDouble[0].name == "elevation");
    EXPECT_TRUE(unitSettings.propertiesDouble[0].value == 4454.0);

    EXPECT_EQ(unitSettings.landCovers.size(), 3);

    EXPECT_TRUE(unitSettings.landCovers[0].name == "ground");
    EXPECT_TRUE(unitSettings.landCovers[0].type == "ground");
    EXPECT_NEAR(unitSettings.landCovers[0].fraction, 0.80616301, 0.00000001);

    EXPECT_TRUE(unitSettings.landCovers[1].name == "glacier-ice");
    EXPECT_TRUE(unitSettings.landCovers[1].type == "glacier");
    EXPECT_NEAR(unitSettings.landCovers[1].fraction, 0.08349901, 0.00000001);

    EXPECT_TRUE(unitSettings.landCovers[2].name == "glacier-debris");
    EXPECT_TRUE(unitSettings.landCovers[2].type == "glacier");
    EXPECT_NEAR(unitSettings.landCovers[2].fraction, 0.11033797, 0.00000001);
}

TEST(SettingsBasin, ParseNcFileWithoutNetworkGivesImplicitSubbasin) {
    SettingsBasin settings;
    EXPECT_TRUE(settings.Parse("files/hydro-units-2-glaciers.nc"));

    EXPECT_EQ(settings.GetSubbasinCount(), 0);
    EXPECT_EQ(settings.GetHydroUnitSettings(0).subbasinId, 1);
    EXPECT_TRUE(settings.ValidateNetwork());
}

TEST(SettingsBasin, ValidateNetworkImplicitSubbasinRejectsOtherIds) {
    SettingsBasin settings;
    settings.AddHydroUnit(1, 100, 1000, 2);

    auto result = settings.ValidateNetwork();
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().find("no subbasin is declared"), string::npos);
}

TEST(SettingsBasin, ValidateNetworkAcceptsTree) {
    SettingsBasin settings;
    settings.AddSubbasin(1, 0);
    settings.AddSubbasin(2, 1);
    settings.AddSubbasin(3, 2);
    settings.AddHydroUnit(1, 100, 1000, 1);
    settings.AddHydroUnit(2, 100, 1000, 2);
    settings.AddHydroUnit(3, 100, 1000, 3);

    EXPECT_TRUE(settings.ValidateNetwork());
}

TEST(SettingsBasin, ValidateNetworkRejectsDuplicateId) {
    SettingsBasin settings;
    settings.AddSubbasin(1, 0);
    settings.AddSubbasin(1, 0);
    settings.AddHydroUnit(1, 100, 1000, 1);

    auto result = settings.ValidateNetwork();
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().find("declared twice"), string::npos);
}

TEST(SettingsBasin, ValidateNetworkRejectsTwoOutlets) {
    SettingsBasin settings;
    settings.AddSubbasin(1, 0);
    settings.AddSubbasin(2, 0);
    settings.AddHydroUnit(1, 100, 1000, 1);
    settings.AddHydroUnit(2, 100, 1000, 2);

    auto result = settings.ValidateNetwork();
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().find("exactly one terminal subbasin"), string::npos);
}

TEST(SettingsBasin, ValidateNetworkRejectsUnknownDownstream) {
    SettingsBasin settings;
    settings.AddSubbasin(1, 0);
    settings.AddSubbasin(2, 5);
    settings.AddHydroUnit(1, 100, 1000, 1);
    settings.AddHydroUnit(2, 100, 1000, 2);

    auto result = settings.ValidateNetwork();
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().find("undeclared subbasin 5"), string::npos);
}

TEST(SettingsBasin, ValidateNetworkRejectsCycle) {
    SettingsBasin settings;
    settings.AddSubbasin(1, 0);
    settings.AddSubbasin(2, 3);
    settings.AddSubbasin(3, 2);
    settings.AddHydroUnit(1, 100, 1000, 1);
    settings.AddHydroUnit(2, 100, 1000, 2);
    settings.AddHydroUnit(3, 100, 1000, 3);

    auto result = settings.ValidateNetwork();
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().find("cycle"), string::npos);
}

TEST(SettingsBasin, ValidateNetworkRejectsEmptySubbasin) {
    SettingsBasin settings;
    settings.AddSubbasin(1, 0);
    settings.AddSubbasin(2, 1);
    settings.AddHydroUnit(1, 100, 1000, 1);

    auto result = settings.ValidateNetwork();
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().find("Subbasin 2 has no hydro unit"), string::npos);
}

TEST(SettingsBasin, ValidateNetworkRejectsUnitOnUnknownSubbasin) {
    SettingsBasin settings;
    settings.AddSubbasin(1, 0);
    settings.AddHydroUnit(1, 100, 1000, 1);
    settings.AddHydroUnit(2, 100, 1000, 4);

    auto result = settings.ValidateNetwork();
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().find("Hydro unit 2 refers to the undeclared subbasin 4"), string::npos);
}

TEST(SettingsBasin, ClearDropsSubbasins) {
    SettingsBasin settings;
    settings.AddSubbasin(1, 0);
    settings.AddHydroUnit(1, 100, 1000, 1);
    settings.Clear();

    EXPECT_EQ(settings.GetSubbasinCount(), 0);
    EXPECT_EQ(settings.GetHydroUnitCount(), 0);
}

TEST(SettingsBasin, ParseNcFileWithNetwork) {
    string path = std::filesystem::temp_directory_path().string() + "/hb_hydro_units_network.nc";
    {
        FileNetcdf file;
        ASSERT_TRUE(file.Create(path));
        int dimUnits = file.DefDim("hydro_units", 3);
        int dimSubbasins = file.DefDim("subbasins", 2);

        int varId = file.DefVarInt("id", {dimUnits});
        file.PutVar(varId, vecInt{1, 2, 3});
        varId = file.DefVarDouble("area", {dimUnits});
        file.PutVar(varId, vecDouble{100.0, 200.0, 300.0});
        varId = file.DefVarDouble("elevation", {dimUnits});
        file.PutVar(varId, vecDouble{1000.0, 1100.0, 1200.0});
        varId = file.DefVarInt("subbasin", {dimUnits});
        file.PutVar(varId, vecInt{2, 2, 1});

        varId = file.DefVarInt("subbasin_id", {dimSubbasins});
        file.PutVar(varId, vecInt{1, 2});
        varId = file.DefVarInt("subbasin_downstream_id", {dimSubbasins});
        file.PutVar(varId, vecInt{0, 1});
        varId = file.DefVarDouble("length", {dimSubbasins});
        file.PutVar(varId, vecDouble{1500.0, 2500.0});
        file.PutAttText("units", "m", varId);
        file.PutAttString("subbasin_names", vecStr{"gauge_down", "gauge_up"});
        file.Close();
    }

    SettingsBasin settings;
    ASSERT_TRUE(settings.Parse(path));
    std::filesystem::remove(path);

    EXPECT_EQ(settings.GetHydroUnitCount(), 3);
    EXPECT_EQ(settings.GetHydroUnitSettings(0).subbasinId, 2);
    EXPECT_EQ(settings.GetHydroUnitSettings(2).subbasinId, 1);

    // The subbasin column is not a hydro unit property; the elevation still is.
    for (const auto& prop : settings.GetHydroUnitSettings(0).propertiesDouble) {
        EXPECT_NE(prop.name, "subbasin");
    }

    ASSERT_EQ(settings.GetSubbasinCount(), 2);
    EXPECT_EQ(settings.GetSubbasinSettings(0).id, 1);
    EXPECT_EQ(settings.GetSubbasinSettings(0).downstreamId, 0);
    EXPECT_EQ(settings.GetSubbasinSettings(0).name, "gauge_down");
    EXPECT_EQ(settings.GetSubbasinSettings(1).id, 2);
    EXPECT_EQ(settings.GetSubbasinSettings(1).downstreamId, 1);
    EXPECT_EQ(settings.GetSubbasinSettings(1).name, "gauge_up");
    ASSERT_EQ(settings.GetSubbasinSettings(1).propertiesDouble.size(), 1);
    EXPECT_EQ(settings.GetSubbasinSettings(1).propertiesDouble[0].name, "length");
    EXPECT_DOUBLE_EQ(settings.GetSubbasinSettings(1).propertiesDouble[0].value, 2500.0);
    EXPECT_EQ(settings.GetSubbasinSettings(1).propertiesDouble[0].unit, "m");

    EXPECT_TRUE(settings.ValidateNetwork());
}
