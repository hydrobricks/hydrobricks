#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <memory>

#include "FileNetcdf.h"
#include "HydroUnit.h"
#include "Logger.h"
#include "ModelHydro.h"
#include "SettingsBasin.h"
#include "SettingsModel.h"
#include "SubBasin.h"
#include "TimeSeriesUniform.h"
#include "Utils.h"

/**
 * Two hydro units using different structure variants: unit 0 uses structure 1
 * (ground only), unit 1 uses structure 2 (ground + an extra storage brick). The
 * model must build per unit, run, and log per structure: a label present only in
 * structure 2 ('extra:water_content') stays NaN for unit 0 (omitted) and is
 * recorded for unit 1, while a shared label is recorded for both.
 */
TEST(MultiStructure, HeterogeneousUnitsBuildAndLogPerStructure) {
    SettingsModel settings;
    settings.SetSolver("heun_explicit");
    settings.SetTimer("2020-01-01", "2020-01-05", 1, "day");
    settings.SetLogAll(true);

    // Structure 1: a single generic land cover draining directly to the outlet.
    settings.GeneratePrecipitationSplitters(false);
    settings.AddLandCoverBrick("ground", "generic_land_cover");
    settings.SelectHydroUnitBrick("ground");
    settings.AddBrickProcess("outflow", "outflow:direct", "outlet");
    settings.AddLoggingToItem("outlet");  // log the catchment outlet (primary structure)

    // Structure 2: same land cover, plus an extra (isolated) storage brick whose
    // log label does not exist in structure 1.
    settings.AddStructure();
    settings.GeneratePrecipitationSplitters(false);
    settings.AddLandCoverBrick("ground", "generic_land_cover");
    settings.SelectHydroUnitBrick("ground");
    settings.AddBrickProcess("outflow", "outflow:direct", "outlet");
    settings.AddHydroUnitBrick("extra", "storage");
    settings.SelectHydroUnitBrick("extra");
    settings.AddBrickProcess("outflow", "outflow:linear", "outlet");

    // Two units, both fully covered by 'ground'.
    SettingsBasin basin;
    basin.AddHydroUnit(1, 100);
    basin.AddLandCover("ground", "", 1.0);
    basin.AddHydroUnit(2, 100);
    basin.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    ASSERT_TRUE(subBasin.Initialize(basin));

    // Assign the second unit (index 1) to structure 2; the first keeps the default 1.
    subBasin.GetHydroUnit(1)->SetStructureId(2);

    ModelHydro model(&subBasin);
    auto initResult = model.Initialize(settings, basin);
    ASSERT_TRUE(initResult.has_value()) << "Initialize failed: " << initResult.error();
    EXPECT_TRUE(model.IsValid());

    auto precip = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 5), 1, TimeUnit::Day);
    precip->SetValues({0.0, 10.0, 10.0, 10.0, 0.0});
    auto tsPrecip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
    tsPrecip->SetData(std::move(precip));
    ASSERT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(tsPrecip))));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    ASSERT_TRUE(model.Run());

    Logger* logger = model.GetLogger();
    const vecStr& labels = logger->GetHydroUnitLabels();
    const vecAxxd& values = logger->GetHydroUnitValues();

    // The union of labels must contain the structure-2-only label and the shared one.
    auto labelIndex = [&labels](const string& name) -> int {
        for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
            if (labels[i] == name) {
                return i;
            }
        }
        return -1;
    };

    int iExtra = labelIndex("extra:water_content");
    int iGround = labelIndex("ground:water_content");
    ASSERT_NE(iExtra, -1);
    ASSERT_NE(iGround, -1);

    // Structure-2-only label: omitted (NaN) for unit 0, recorded for unit 1.
    for (int t = 0; t < values[iExtra].rows(); ++t) {
        EXPECT_TRUE(std::isnan(values[iExtra](t, 0)));
        EXPECT_FALSE(std::isnan(values[iExtra](t, 1)));
    }

    // Shared label: recorded for both units.
    for (int t = 0; t < values[iGround].rows(); ++t) {
        EXPECT_FALSE(std::isnan(values[iGround](t, 0)));
        EXPECT_FALSE(std::isnan(values[iGround](t, 1)));
    }

    // Aggregation must be NaN-safe despite the omitted cells: totals stay finite and
    // the water balance closes (the extra storage of unit 1 has no inflow).
    double discharge = logger->GetTotalOutletDischarge();
    double et = logger->GetTotalET();
    double waterStorage = logger->GetTotalWaterStorageChanges();
    EXPECT_FALSE(std::isnan(discharge));
    EXPECT_FALSE(std::isnan(et));
    EXPECT_FALSE(std::isnan(waterStorage));
    const double precipDepth = 30.0;  // catchment-average precipitation depth [mm]
    EXPECT_NEAR(discharge + et + waterStorage - precipDepth, 0.0, 1e-6)
        << "discharge=" << discharge << " et=" << et << " storage=" << waterStorage;

    // The per-unit structure id must be written to the NetCDF output.
    string outDir = std::filesystem::temp_directory_path().string();
    ASSERT_TRUE(model.DumpOutputs(outDir));
    FileNetcdf file;
    ASSERT_TRUE(file.OpenReadOnly((std::filesystem::path(outDir) / "results.nc").string()));
    vecInt structureIds = file.GetVarInt1D("hydro_units_structure_ids", 2);
    ASSERT_EQ(structureIds.size(), 2);
    EXPECT_EQ(structureIds[0], 1);
    EXPECT_EQ(structureIds[1], 2);
}

/**
 * Structure variants are auto-assigned from the land covers each unit actually has:
 * a unit with an extra cover uses the full variant (and carries that cover's brick),
 * a unit without it uses the reduced variant (and does not carry the brick at all).
 */
TEST(MultiStructure, StructureAutoAssignedFromLandCovers) {
    SettingsModel settings;
    settings.SetSolver("heun_explicit");
    settings.SetTimer("2020-01-01", "2020-01-05", 1, "day");

    // Structure 1 (full): two land covers.
    settings.GeneratePrecipitationSplitters(false);
    settings.AddLandCoverBrick("ground", "generic_land_cover");
    settings.SelectHydroUnitBrick("ground");
    settings.AddBrickProcess("outflow", "outflow:direct", "outlet");
    settings.AddLandCoverBrick("forest", "generic_land_cover");
    settings.SelectHydroUnitBrick("forest");
    settings.AddBrickProcess("outflow", "outflow:direct", "outlet");

    // Structure 2 (reduced): ground only.
    settings.AddStructure();
    settings.GeneratePrecipitationSplitters(false);
    settings.AddLandCoverBrick("ground", "generic_land_cover");
    settings.SelectHydroUnitBrick("ground");
    settings.AddBrickProcess("outflow", "outflow:direct", "outlet");

    // Unit 0 has both covers; unit 1 has only ground.
    SettingsBasin basin;
    basin.AddHydroUnit(1, 100);
    basin.AddLandCover("ground", "generic_land_cover", 0.6);
    basin.AddLandCover("forest", "generic_land_cover", 0.4);
    basin.AddHydroUnit(2, 100);
    basin.AddLandCover("ground", "generic_land_cover", 1.0);

    // InitializeWithBasin builds the SubBasin and auto-assigns structures from covers.
    ModelHydro model;
    auto initResult = model.InitializeWithBasin(settings, basin);
    ASSERT_TRUE(initResult.has_value()) << "Initialize failed: " << initResult.error();
    SubBasin* subBasin = model.GetSubBasin();

    // Units matched the variant that fits their land covers.
    EXPECT_EQ(subBasin->GetHydroUnit(0)->GetStructureId(), 1);
    EXPECT_EQ(subBasin->GetHydroUnit(1)->GetStructureId(), 2);

    // The 'forest' brick exists only where forest is present (no zero-area brick).
    EXPECT_TRUE(subBasin->GetHydroUnit(0)->HasBrick("forest"));
    EXPECT_FALSE(subBasin->GetHydroUnit(1)->HasBrick("forest"));
    EXPECT_TRUE(subBasin->GetHydroUnit(1)->HasBrick("ground"));
}
