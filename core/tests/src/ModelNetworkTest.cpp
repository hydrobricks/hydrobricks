#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>

#include "FileNetcdf.h"
#include "ModelHydro.h"
#include "SettingsBasin.h"
#include "SettingsModel.h"
#include "TimeSeriesDistributed.h"
#include "TimeSeriesUniform.h"
#include "Utils.h"

namespace {

constexpr int kSteps = 12;

/**
 * A minimal structure with a sub basin-level store: every hydro unit's ground cover drains directly into a
 * catchment-level linear storage, which drains to the outlet. Linear stores make the lumped and the split
 * catchments equivalent (superposition), which the tests rely on.
 */
void BuildStructure(SettingsModel& settings) {
    settings.SetSolver("heun_explicit");
    settings.SetTimer("2020-01-01", "2020-01-12", 1, "day");
    settings.SetLogAll(true);

    settings.GeneratePrecipitationSplitters(false);
    settings.AddLandCoverBrick("ground", "generic_land_cover");
    settings.SelectHydroUnitBrick("ground");
    settings.AddBrickProcess("outflow", "outflow:direct", "store");

    settings.AddSubBasinBrick("store", "storage");
    settings.SelectSubBasinBrick("store");
    settings.AddBrickProcess("outflow", "outflow:linear", "outlet");
    settings.AddProcessParameter("response_factor", 0.3f);

    settings.AddLoggingToItem("outlet");
}

// Three units of different areas; units 1 and 2 form the downstream subbasin, unit 3 the upstream one.
const vecInt kUnitIds = {1, 2, 3};
const vecDouble kUnitAreas = {100.0, 300.0, 600.0};
const vecInt kUnitSubbasins = {1, 1, 2};

void AddUnits(SettingsBasin& basin, const vecInt& ids, bool withNetwork) {
    for (int id : ids) {
        int i = id - 1;
        basin.AddHydroUnit(id, kUnitAreas[i], 1000.0, withNetwork ? kUnitSubbasins[i] : 1);
        basin.AddLandCover("ground", "", 1.0);
    }
}

// Precipitation differs per unit so that the area weighting matters: 10 mm on unit 1 and 3 for four days,
// 20 mm on unit 2 for two days, then dry.
void AttachForcing(ModelHydro& model, const vecInt& ids) {
    auto ts = std::make_unique<TimeSeriesDistributed>(VariableType::Precipitation);
    for (int id : ids) {
        auto data = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 12), 1, TimeUnit::Day);
        vecDouble values(kSteps, 0.0);
        if (id == 2) {
            values[1] = 20.0;
            values[2] = 20.0;
        } else {
            values[1] = 10.0;
            values[2] = 10.0;
            values[3] = 10.0;
            values[4] = 10.0;
        }
        data->SetValues(values);
        ts->AddData(std::move(data), id);
    }
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(ts))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());
}

double MeanPrecipitation(const vecInt& ids) {
    double sum = 0;
    double area = 0;
    for (int id : ids) {
        int i = id - 1;
        sum += (id == 2 ? 40.0 : 40.0) * kUnitAreas[i];
        area += kUnitAreas[i];
    }
    return sum / area;
}

}  // namespace

TEST(ModelNetwork, SplitCatchmentReproducesLumpedOutlet) {
    // Lumped: one subbasin with the three units.
    SettingsModel lumpedSettings;
    BuildStructure(lumpedSettings);
    SettingsBasin lumpedBasin;
    AddUnits(lumpedBasin, kUnitIds, false);
    ModelHydro lumped;
    ASSERT_TRUE(lumped.InitializeWithBasin(lumpedSettings, lumpedBasin));
    AttachForcing(lumped, kUnitIds);
    ASSERT_TRUE(lumped.Run());

    // Network: unit 3 upstream (subbasin 2) draining into subbasin 1 (units 1 and 2).
    SettingsModel networkSettings;
    BuildStructure(networkSettings);
    SettingsBasin networkBasin;
    networkBasin.AddSubbasin(1, 0, "outlet");
    networkBasin.AddSubbasin(2, 1, "upstream");
    AddUnits(networkBasin, kUnitIds, true);
    ModelHydro network;
    ASSERT_TRUE(network.InitializeWithBasin(networkSettings, networkBasin));
    AttachForcing(network, kUnitIds);
    ASSERT_TRUE(network.Run());

    EXPECT_EQ(network.GetSubbasinCount(), 2);
    EXPECT_EQ(lumped.GetSubbasinCount(), 1);

    axd lumpedOutlet = lumped.GetOutletDischarge();
    axd networkOutlet = network.GetOutletDischarge();
    ASSERT_EQ(lumpedOutlet.size(), kSteps);
    ASSERT_EQ(networkOutlet.size(), kSteps);
    for (int t = 0; t < kSteps; ++t) {
        EXPECT_NEAR(networkOutlet[t], lumpedOutlet[t], 1e-10) << "t=" << t;
    }
    EXPECT_NEAR(network.GetTotalOutletDischarge(), lumped.GetTotalOutletDischarge(), 1e-9);
    EXPECT_NEAR(network.GetTotalWaterStorageChanges(), lumped.GetTotalWaterStorageChanges(), 1e-9);

    // Water balance of the network, in mm over the catchment: precipitation = outlet + storage change.
    double precip = MeanPrecipitation(kUnitIds);
    EXPECT_NEAR(network.GetTotalOutletDischarge() + network.GetTotalWaterStorageChanges(), precip, 1e-6);
}

TEST(ModelNetwork, UpstreamSubbasinDischargeMatchesStandaloneRun) {
    // The upstream subbasin alone (unit 3).
    SettingsModel aloneSettings;
    BuildStructure(aloneSettings);
    SettingsBasin aloneBasin;
    AddUnits(aloneBasin, {3}, false);
    ModelHydro alone;
    ASSERT_TRUE(alone.InitializeWithBasin(aloneSettings, aloneBasin));
    AttachForcing(alone, {3});
    ASSERT_TRUE(alone.Run());

    SettingsModel networkSettings;
    BuildStructure(networkSettings);
    SettingsBasin networkBasin;
    networkBasin.AddSubbasin(1, 0, "outlet");
    networkBasin.AddSubbasin(2, 1, "upstream");
    AddUnits(networkBasin, kUnitIds, true);
    ModelHydro network;
    ASSERT_TRUE(network.InitializeWithBasin(networkSettings, networkBasin));
    AttachForcing(network, kUnitIds);
    ASSERT_TRUE(network.Run());

    axd aloneOutlet = alone.GetOutletDischarge();
    axd upstream = network.GetSubbasinDischarge(2);
    for (int t = 0; t < kSteps; ++t) {
        EXPECT_NEAR(upstream[t], aloneOutlet[t], 1e-12) << "t=" << t;
    }

    // The downstream outlet, over the drained area, is the area-weighted mix of the upstream discharge and the
    // local runoff of the downstream subbasin.
    vecInt ids = network.GetSubbasinIds();
    ASSERT_EQ(ids.size(), 2);
    EXPECT_EQ(ids.back(), 1);  // outlet last
    EXPECT_EQ(ids.front(), 2);
    axd local = network.GetSubbasinLocalAreas();
    axd drained = network.GetSubbasinDrainedAreas();
    EXPECT_DOUBLE_EQ(local[0], 600.0);
    EXPECT_DOUBLE_EQ(drained[0], 600.0);
    EXPECT_DOUBLE_EQ(local[1], 400.0);
    EXPECT_DOUBLE_EQ(drained[1], 1000.0);
    vecInt downstream = network.GetSubbasinDownstreamIds();
    EXPECT_EQ(downstream[0], 1);
    EXPECT_EQ(downstream[1], 0);
}

TEST(ModelNetwork, ResultsFileCarriesTheSubbasinDimension) {
    SettingsModel settings;
    BuildStructure(settings);
    SettingsBasin basin;
    basin.AddSubbasin(1, 0, "outlet");
    basin.AddSubbasin(2, 1, "upstream");
    AddUnits(basin, kUnitIds, true);
    ModelHydro model;
    ASSERT_TRUE(model.InitializeWithBasin(settings, basin));
    AttachForcing(model, kUnitIds);
    ASSERT_TRUE(model.Run());

    string outDir = (std::filesystem::temp_directory_path() / "hb_network_test").string();
    std::filesystem::create_directories(outDir);
    ASSERT_TRUE(model.DumpOutputs(outDir));

    FileNetcdf file;
    ASSERT_TRUE(file.OpenReadOnly((std::filesystem::path(outDir) / "results.nc").string()));
    EXPECT_EQ(file.GetDimLen("subbasins"), 2);
    vecInt ids = file.GetVarInt1D("subbasin_ids", 2);
    EXPECT_EQ(ids[0], 2);
    EXPECT_EQ(ids[1], 1);
    vecInt downstream = file.GetVarInt1D("subbasin_downstream_ids", 2);
    EXPECT_EQ(downstream[0], 1);
    EXPECT_EQ(downstream[1], 0);
    vecDouble drained = file.GetVarDouble1D("subbasin_drained_areas", 2);
    EXPECT_DOUBLE_EQ(drained[1], 1000.0);
    EXPECT_EQ(file.GetAttText("format_version"), "2");

    // subbasin_values[aggregated_values, subbasins, time]: the outlet row of the terminal subbasin equals the
    // outlet discharge.
    vecStr labels = file.GetAttString1D("labels_aggregated");
    int iOutlet = -1;
    for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
        if (labels[i] == "outlet") iOutlet = i;
    }
    ASSERT_GE(iOutlet, 0);
    // The file is row-major [label][subbasin][time]; read it as a column-major (time x rows) array so that
    // values(t, label * subbasinCount + subbasin) addresses one cell.
    int varId = file.GetVarId("subbasin_values");
    axxd values = file.GetVarDouble2D(varId, kSteps, 2 * static_cast<int>(labels.size()));
    axd outlet = model.GetOutletDischarge();
    axd upstream = model.GetSubbasinDischarge(2);
    for (int t = 0; t < kSteps; ++t) {
        EXPECT_NEAR(values(t, iOutlet * 2 + 0), upstream[t], 1e-12) << "t=" << t;
        EXPECT_NEAR(values(t, iOutlet * 2 + 1), outlet[t], 1e-12) << "t=" << t;
    }
    file.Close();
    std::filesystem::remove_all(outDir);
}

TEST(ModelNetwork, LagRoutingDelaysTheUpstreamWaterByOneStep) {
    // Upstream subbasin 2 drains through a reach of 86.4 km at 1 m/s: one day of travel time.
    auto build = [](const string& scheme, ModelHydro& model, SettingsModel& settings, SettingsBasin& basin) {
        BuildStructure(settings);
        settings.SetChannelRouting(scheme);
        settings.SetParameterValue("channel", "celerity", 1.0f);
        basin.AddSubbasin(1, 0, "outlet");
        basin.AddSubbasinPropertyDouble("length", 86400.0, "m");
        basin.AddSubbasin(2, 1, "upstream");
        basin.AddSubbasinPropertyDouble("length", 5000.0, "m");
        AddUnits(basin, kUnitIds, true);
        ASSERT_TRUE(model.InitializeWithBasin(settings, basin));
        AttachForcing(model, kUnitIds);
        ASSERT_TRUE(model.Run());
    };

    SettingsModel noneSettings;
    SettingsBasin noneBasin;
    ModelHydro none;
    build("none", none, noneSettings, noneBasin);

    SettingsModel lagSettings;
    SettingsBasin lagBasin;
    ModelHydro lag;
    build("lag", lag, lagSettings, lagBasin);

    // The upstream subbasin is unaffected (its own reach carries nothing), the outlet receives the upstream
    // water one step later: outlet_lag(t) = outlet_none(t) + (up(t-1) - up(t)) * A2 / A_drained.
    axd upNone = none.GetSubbasinDischarge(2);
    axd upLag = lag.GetSubbasinDischarge(2);
    axd outNone = none.GetOutletDischarge();
    axd outLag = lag.GetOutletDischarge();
    double ratio = 600.0 / 1000.0;
    for (int t = 0; t < kSteps; ++t) {
        EXPECT_NEAR(upLag[t], upNone[t], 1e-12) << "t=" << t;
        double previous = t > 0 ? upNone[t - 1] : 0.0;
        EXPECT_NEAR(outLag[t], outNone[t] + (previous - upNone[t]) * ratio, 1e-10) << "t=" << t;
    }

    // Mass: the water still in the reach at the end explains the difference of the totals.
    double inTransit = lag.GetNetwork()->GetOutlet()->GetReach()->GetStorage() / 1000.0;
    EXPECT_NEAR(lag.GetTotalOutletDischarge() + inTransit, none.GetTotalOutletDischarge(), 1e-9);
}

TEST(ModelNetwork, ReachValuesAreLoggedWhenRequested) {
    SettingsModel settings;
    BuildStructure(settings);
    settings.SetChannelRouting("lag");
    settings.AddLoggingToItems({"reach:inflow", "reach:outflow", "reach:storage"});
    SettingsBasin basin;
    basin.AddSubbasin(1, 0, "outlet");
    basin.AddSubbasinPropertyDouble("length", 86400.0, "m");
    basin.AddSubbasin(2, 1, "upstream");
    AddUnits(basin, kUnitIds, true);
    ModelHydro model;
    ASSERT_TRUE(model.InitializeWithBasin(settings, basin));
    AttachForcing(model, kUnitIds);
    ASSERT_TRUE(model.Run());

    const vecStr& labels = model.GetLogger()->GetSubBasinLabels();
    auto index = [&labels](const string& name) {
        return static_cast<int>(std::find(labels.begin(), labels.end(), name) - labels.begin());
    };
    const vecAxxd& values = model.GetLogger()->GetSubBasinValuesPerSubbasin();
    ASSERT_LT(index("reach:inflow"), static_cast<int>(labels.size()));
    axd inflow = values[index("reach:inflow")].col(1);  // the outlet subbasin (last)
    axd outflow = values[index("reach:outflow")].col(1);
    axd upstream = model.GetSubbasinDischarge(2);
    for (int t = 0; t < kSteps; ++t) {
        // The reach inflow is the upstream discharge, over the drained area of the outlet subbasin.
        EXPECT_NEAR(inflow[t], upstream[t] * 600.0 / 1000.0, 1e-12) << "t=" << t;
        double previous = t > 0 ? inflow[t - 1] : 0.0;
        EXPECT_NEAR(outflow[t], previous, 1e-12) << "t=" << t;
    }
    // The headwater has no upstream: its reach values stay at zero.
    EXPECT_DOUBLE_EQ(values[index("reach:inflow")].col(0).sum(), 0.0);
}

TEST(ModelNetwork, SubbasinsBuildTheVariantMatchingTheirLandCovers) {
    // Variant 1: ground only, draining directly to the outlet (no subbasin-level brick).
    SettingsModel settings;
    settings.SetSolver("heun_explicit");
    settings.SetTimer("2020-01-01", "2020-01-12", 1, "day");
    settings.SetLogAll(true);
    settings.GeneratePrecipitationSplitters(false);
    settings.AddLandCoverBrick("ground", "generic_land_cover");
    settings.SelectHydroUnitBrick("ground");
    settings.AddBrickProcess("outflow", "outflow:direct", "outlet");
    settings.AddLoggingToItem("outlet");

    // Variant 2: ground and forest; the forest drains through a subbasin-level linear store.
    settings.AddStructure();
    settings.GeneratePrecipitationSplitters(false);
    settings.AddLandCoverBrick("ground", "generic_land_cover");
    settings.SelectHydroUnitBrick("ground");
    settings.AddBrickProcess("outflow", "outflow:direct", "outlet");
    settings.AddLandCoverBrick("forest", "generic_land_cover");
    settings.SelectHydroUnitBrick("forest");
    settings.AddBrickProcess("outflow", "outflow:direct", "store");
    settings.AddSubBasinBrick("store", "storage");
    settings.SelectSubBasinBrick("store");
    settings.AddBrickProcess("outflow", "outflow:linear", "outlet");
    settings.AddProcessParameter("response_factor", 0.3f);
    settings.AddLoggingToItem("outlet");

    // Subbasin 1 (outlet) has a unit with forest; subbasin 2 (upstream) has ground only.
    SettingsBasin basin;
    basin.AddSubbasin(1, 0, "outlet");
    basin.AddSubbasin(2, 1, "upstream");
    basin.AddHydroUnit(1, 100, 1000, 1);
    basin.AddLandCover("ground", "generic_land_cover", 0.5);
    basin.AddLandCover("forest", "generic_land_cover", 0.5);
    basin.AddHydroUnit(2, 300, 1000, 2);
    basin.AddLandCover("ground", "generic_land_cover", 1.0);

    ModelHydro model;
    ASSERT_TRUE(model.InitializeWithBasin(settings, basin));
    RiverNetwork* network = model.GetNetwork();
    EXPECT_EQ(network->GetSubbasinById(1)->GetStructureId(), 2);
    EXPECT_EQ(network->GetSubbasinById(2)->GetStructureId(), 1);
    EXPECT_TRUE(network->GetSubbasinById(1)->HasBrick("store"));
    EXPECT_FALSE(network->GetSubbasinById(2)->HasBrick("store"));

    auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 12), 1, TimeUnit::Day);
    vecDouble values(kSteps, 0.0);
    values[1] = 10.0;
    values[2] = 10.0;
    precip->SetValues(values);
    auto ts = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
    ts->SetData(std::move(precip));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(ts))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());
    ASSERT_TRUE(model.Run());

    // The store label is recorded for the outlet subbasin only (NaN for the upstream one), and the
    // subbasin structure ids are reported in processing order (upstream first).
    Logger* logger = model.GetLogger();
    const vecStr& labels = logger->GetSubBasinLabels();
    auto it = std::find(labels.begin(), labels.end(), "store:water_content");
    ASSERT_NE(it, labels.end());
    int iStore = static_cast<int>(it - labels.begin());
    const axxd& store = logger->GetSubBasinValuesPerSubbasin()[iStore];
    EXPECT_TRUE(std::isnan(store(kSteps - 1, 0)));   // upstream (subbasin 2)
    EXPECT_FALSE(std::isnan(store(kSteps - 1, 1)));  // outlet (subbasin 1)
    EXPECT_GT(store(3, 1), 0.0);
    vecInt structureIds = logger->GetSubbasinStructureIds();
    ASSERT_EQ(structureIds.size(), 2);
    EXPECT_EQ(structureIds[0], 1);
    EXPECT_EQ(structureIds[1], 2);

    // Totals stay finite and the water balance closes despite the missing label.
    double outlet = model.GetTotalOutletDischarge();
    double storage = model.GetTotalWaterStorageChanges();
    EXPECT_FALSE(std::isnan(outlet));
    EXPECT_NEAR(outlet + storage, 20.0, 1e-6);
}
