#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "RiverNetwork.h"
#include "SettingsBasin.h"
#include "SettingsModel.h"

namespace {

/**
 * A four-subbasin tree:
 *
 *   4 (headwater) -> 3 -> 1 (outlet) <- 2 (headwater)
 *
 * Unit areas: subbasin 1 holds units 1, 2 (100 + 200); subbasin 2 holds unit 3 (300);
 * subbasin 3 holds unit 4 (400); subbasin 4 holds units 5, 6 (500 + 600).
 */
SettingsBasin BuildTreeSettings() {
    SettingsBasin settings;
    settings.AddSubbasin(1, 0, "outlet");
    settings.AddSubbasinPropertyDouble("length", 1000.0, "m");
    settings.AddSubbasin(2, 1, "left");
    settings.AddSubbasinPropertyDouble("length", 2000.0, "m");
    settings.AddSubbasin(3, 1, "right");
    settings.AddSubbasinPropertyDouble("length", 3000.0, "m");
    settings.AddSubbasin(4, 3, "right-head");
    settings.AddSubbasinPropertyDouble("length", 4000.0, "m");
    settings.AddSubbasinPropertyString("gauge", "none");

    settings.AddHydroUnit(1, 100, 1000, 1);
    settings.AddLandCover("ground", "", 1.0);
    settings.AddHydroUnit(2, 200, 1100, 1);
    settings.AddLandCover("ground", "", 1.0);
    settings.AddHydroUnit(3, 300, 1200, 2);
    settings.AddLandCover("ground", "", 1.0);
    settings.AddHydroUnit(4, 400, 1300, 3);
    settings.AddLandCover("ground", "", 1.0);
    settings.AddHydroUnit(5, 500, 1400, 4);
    settings.AddLandCover("ground", "", 1.0);
    settings.AddHydroUnit(6, 600, 1500, 4);
    settings.AddLandCover("ground", "", 1.0);

    return settings;
}

int IndexInOrder(const std::vector<SubBasin*>& order, int id) {
    for (int i = 0; i < static_cast<int>(order.size()); ++i) {
        if (order[i]->GetId() == id) return i;
    }
    return -1;
}

}  // namespace

TEST(RiverNetwork, ImplicitSingleSubbasinHoldsEveryUnit) {
    SettingsBasin settings;
    settings.AddHydroUnit(1, 100);
    settings.AddLandCover("ground", "", 1.0);
    settings.AddHydroUnit(2, 300);
    settings.AddLandCover("ground", "", 1.0);

    RiverNetwork network;
    ASSERT_TRUE(network.Initialize(settings));

    EXPECT_EQ(network.GetSubbasinCount(), 1);
    ASSERT_NE(network.GetOutlet(), nullptr);
    EXPECT_EQ(network.GetOutlet()->GetId(), 1);
    EXPECT_TRUE(network.GetOutlet()->IsTerminal());
    EXPECT_EQ(network.GetOutlet()->GetHydroUnitCount(), 2);
    EXPECT_DOUBLE_EQ(network.GetOutlet()->GetLocalArea(), 400.0);
    EXPECT_DOUBLE_EQ(network.GetOutlet()->GetDrainedArea(), 400.0);
    EXPECT_DOUBLE_EQ(network.GetTotalArea(), 400.0);
    EXPECT_EQ(network.GetProcessingOrder().size(), 1);
}

TEST(RiverNetwork, TreeBuildsSubbasinsWithTheirUnits) {
    SettingsBasin settings = BuildTreeSettings();

    RiverNetwork network;
    ASSERT_TRUE(network.Initialize(settings));

    EXPECT_EQ(network.GetSubbasinCount(), 4);
    EXPECT_EQ(network.GetHydroUnitCount(), 6);
    ASSERT_NE(network.GetOutlet(), nullptr);
    EXPECT_EQ(network.GetOutlet()->GetId(), 1);
    EXPECT_EQ(network.GetOutlet()->GetName(), "outlet");

    EXPECT_EQ(network.GetSubbasinById(1)->GetHydroUnitCount(), 2);
    EXPECT_EQ(network.GetSubbasinById(2)->GetHydroUnitCount(), 1);
    EXPECT_EQ(network.GetSubbasinById(3)->GetHydroUnitCount(), 1);
    EXPECT_EQ(network.GetSubbasinById(4)->GetHydroUnitCount(), 2);
    EXPECT_EQ(network.GetSubbasinById(4)->GetDownstreamId(), 3);
    EXPECT_EQ(network.GetSubbasinById(99), nullptr);

    // Units are found across subbasins, by their global IDs.
    ASSERT_NE(network.GetHydroUnitById(5), nullptr);
    EXPECT_DOUBLE_EQ(network.GetHydroUnitById(5)->GetArea(), 500.0);
    EXPECT_EQ(network.GetSubbasinById(4)->GetHydroUnitById(5), network.GetHydroUnitById(5));
}

TEST(RiverNetwork, TreeProcessingOrderIsUpstreamFirst) {
    SettingsBasin settings = BuildTreeSettings();

    RiverNetwork network;
    ASSERT_TRUE(network.Initialize(settings));

    const auto& order = network.GetProcessingOrder();
    ASSERT_EQ(order.size(), 4);
    EXPECT_LT(IndexInOrder(order, 4), IndexInOrder(order, 3));
    EXPECT_LT(IndexInOrder(order, 3), IndexInOrder(order, 1));
    EXPECT_LT(IndexInOrder(order, 2), IndexInOrder(order, 1));
    EXPECT_EQ(order.back()->GetId(), 1);

    auto upstreamOfOutlet = network.GetUpstreamSubbasins(network.GetOutlet());
    ASSERT_EQ(upstreamOfOutlet.size(), 2);
    EXPECT_TRUE(network.GetUpstreamSubbasins(network.GetSubbasinById(4)).empty());
}

TEST(RiverNetwork, TreeDrainedAreasAccumulateUpstream) {
    SettingsBasin settings = BuildTreeSettings();

    RiverNetwork network;
    ASSERT_TRUE(network.Initialize(settings));

    EXPECT_DOUBLE_EQ(network.GetSubbasinById(4)->GetLocalArea(), 1100.0);
    EXPECT_DOUBLE_EQ(network.GetSubbasinById(4)->GetDrainedArea(), 1100.0);
    EXPECT_DOUBLE_EQ(network.GetSubbasinById(3)->GetLocalArea(), 400.0);
    EXPECT_DOUBLE_EQ(network.GetSubbasinById(3)->GetDrainedArea(), 1500.0);
    EXPECT_DOUBLE_EQ(network.GetSubbasinById(2)->GetDrainedArea(), 300.0);
    EXPECT_DOUBLE_EQ(network.GetSubbasinById(1)->GetLocalArea(), 300.0);
    EXPECT_DOUBLE_EQ(network.GetSubbasinById(1)->GetDrainedArea(), 2100.0);
    EXPECT_DOUBLE_EQ(network.GetTotalArea(), 2100.0);
    EXPECT_DOUBLE_EQ(network.GetTotalArea(), settings.GetTotalArea());

    vecDouble drained = network.GetSubbasinDrainedAreas();
    ASSERT_EQ(drained.size(), 4);
    EXPECT_DOUBLE_EQ(drained[0], 2100.0);
    EXPECT_DOUBLE_EQ(drained[3], 1100.0);
}

TEST(RiverNetwork, TreeSubbasinPropertiesAreAvailable) {
    SettingsBasin settings = BuildTreeSettings();

    RiverNetwork network;
    ASSERT_TRUE(network.Initialize(settings));

    SubBasin* head = network.GetSubbasinById(4);
    EXPECT_TRUE(head->HasProperty("length"));
    EXPECT_FALSE(head->HasProperty("slope"));
    EXPECT_DOUBLE_EQ(head->GetPropertyDouble("length"), 4000.0);
    EXPECT_EQ(head->GetPropertyString("gauge"), "none");
    EXPECT_THROW(head->GetPropertyDouble("slope"), ModelConfigError);
}

TEST(RiverNetwork, LateralConnectionsCrossSubbasins) {
    SettingsBasin settings = BuildTreeSettings();
    // Unit 5 (subbasin 4) gives to unit 4 (subbasin 3).
    settings.AddLateralConnection(5, 4, 1.0);

    RiverNetwork network;
    ASSERT_TRUE(network.Initialize(settings));

    HydroUnit* giver = network.GetHydroUnitById(5);
    ASSERT_EQ(giver->GetLateralConnections().size(), 1);
    EXPECT_EQ(giver->GetLateralConnections()[0]->GetReceiver(), network.GetHydroUnitById(4));
}

TEST(RiverNetwork, InvalidNetworkIsRefused) {
    SettingsBasin settings = BuildTreeSettings();
    settings.AddHydroUnit(7, 100, 1000, 8);  // undeclared subbasin
    settings.AddLandCover("ground", "", 1.0);

    RiverNetwork network;
    auto result = network.Initialize(settings);
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().find("undeclared subbasin 8"), string::npos);
}

TEST(ModelHydro, DeclaredSingleSubbasinBuildsAsBefore) {
    SettingsModel modelSettings;
    modelSettings.SetSolver("heun_explicit");
    modelSettings.SetTimer("2020-01-01", "2020-01-05", 1, "day");
    modelSettings.GeneratePrecipitationSplitters(false);
    modelSettings.AddLandCoverBrick("ground", "generic_land_cover");
    modelSettings.SelectHydroUnitBrick("ground");
    modelSettings.AddBrickProcess("outflow", "outflow:direct", "outlet");
    modelSettings.AddLoggingToItem("outlet");

    SettingsBasin basinSettings;
    basinSettings.AddSubbasin(7, 0, "gauge");
    basinSettings.AddHydroUnit(1, 100, 1000, 7);
    basinSettings.AddLandCover("ground", "", 1.0);
    basinSettings.AddHydroUnit(2, 300, 1100, 7);
    basinSettings.AddLandCover("ground", "", 1.0);

    ModelHydro model;
    ASSERT_TRUE(model.InitializeWithBasin(modelSettings, basinSettings));
    ASSERT_NE(model.GetNetwork(), nullptr);
    EXPECT_EQ(model.GetNetwork()->GetSubbasinCount(), 1);
    EXPECT_EQ(model.GetSubBasin(), model.GetNetwork()->GetOutlet());
    EXPECT_EQ(model.GetSubBasin()->GetId(), 7);
    EXPECT_EQ(model.GetSubBasin()->GetName(), "gauge");
    EXPECT_EQ(model.GetSubBasin()->GetHydroUnitCount(), 2);
    EXPECT_DOUBLE_EQ(model.GetSubBasin()->GetDrainedArea(), 400.0);
}
