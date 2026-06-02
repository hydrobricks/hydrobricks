#include "helpers.h"

TimeMachine GenerateTimeMachineDaily() {
    TimeMachine timer;
    timer.Initialize(GetMJD(2020, 1, 1), GetMJD(2020, 1, 31), 1, TimeUnit::Day);
    return timer;
}

bool GenerateStructureSocont(SettingsModel& settings, vecStr& landCoverTypes, vecStr& landCoverNames, int soilStorageNb,
                             const string& surfaceRunoff, bool infiniteGlacierStorage) {
    if (landCoverNames.size() != landCoverTypes.size()) {
        LogError("The length of the land cover names and types do not match.");
        return false;
    }

    // Precipitation
    settings.GeneratePrecipitationSplitters(true);

    // Add default ground land cover
    settings.AddLandCoverBrick("ground", "generic_land_cover");

    // Add other specific land covers
    for (int i = 0; i < landCoverNames.size(); ++i) {
        string type = landCoverTypes[i];
        if (type == "ground" || type == "generic_land_cover") {
            // Nothing to do, already added.
        } else if (type == "glacier") {
            settings.AddLandCoverBrick(landCoverNames[i], "glacier");
        } else {
            LogError("The land cover type {} is not used in Socont", type);
            return false;
        }
    }

    // Snowpacks
    settings.GenerateSnowpacks("melt:degree_day");

    // Add surface-related processes
    for (int i = 0; i < landCoverNames.size(); ++i) {
        string type = landCoverTypes[i];
        string name = landCoverNames[i];

        if (type == "glacier") {
            // Direct rain and snow melt to linear storage
            settings.SelectHydroUnitBrick(name);
            settings.AddBrickProcess("outflow_rain_snowmelt", "outflow:direct", "glacier_area_rain_snowmelt_storage");

            // Glacier melt process
            settings.AddBrickParameter("no_melt_when_snow_cover", 1.0);
            if (infiniteGlacierStorage) {
                settings.AddBrickParameter("infinite_storage", 1.0);
            }
            settings.AddBrickProcess("melt", "melt:degree_day", "glacier_area_icemelt_storage");
            settings.SetProcessOutputsAsInstantaneous();
        }
    }

    // Basin storages for contributions from the glacierized area
    settings.AddSubBasinBrick("glacier_area_rain_snowmelt_storage", "storage");
    settings.AddBrickProcess("outflow", "outflow:linear", "outlet");
    settings.AddSubBasinBrick("glacier_area_icemelt_storage", "storage");
    settings.AddBrickProcess("outflow", "outflow:linear", "outlet");

    // Infiltration and overflow
    settings.SelectHydroUnitBrick("ground");
    settings.AddBrickProcess("infiltration", "infiltration:socont", "slow_reservoir");
    settings.AddBrickProcess("runoff", "outflow:rest_direct", "surface_runoff");
    settings.SetProcessOutputsAsStatic();

    // Add other bricks
    if (soilStorageNb == 1) {
        settings.AddHydroUnitBrick("slow_reservoir", "storage");
        settings.AddBrickParameter("capacity", 200.0f);
        settings.AddBrickProcess("et", "et:socont");
        settings.AddBrickProcess("outflow", "outflow:linear", "outlet");
        settings.AddBrickProcess("overflow", "overflow", "outlet");
    } else if (soilStorageNb == 2) {
        LogMessage("Using 2 soil storages.");
        settings.AddHydroUnitBrick("slow_reservoir", "storage");
        settings.AddBrickParameter("capacity", 200.0f);
        settings.AddBrickProcess("et", "et:socont");
        settings.AddBrickProcess("outflow", "outflow:linear", "outlet");
        settings.AddBrickProcess("percolation", "percolation:constant", "slow_reservoir_2");
        settings.AddBrickProcess("overflow", "overflow", "outlet");
        settings.AddHydroUnitBrick("slow_reservoir_2", "storage");
        settings.AddBrickProcess("outflow", "outflow:linear", "outlet");
        settings.SetProcessParameterValue("response_factor", 0.02f);
    } else {
        LogError("There can be only one or two groundwater storages.");
    }

    settings.AddHydroUnitBrick("surface_runoff", "storage");
    if (surfaceRunoff == "socont_runoff") {
        settings.AddBrickProcess("runoff", "runoff:socont", "outlet");
    } else if (surfaceRunoff == "linear_storage") {
        LogMessage("Using a linear storage for the quick flow.");
        settings.AddBrickProcess("outflow", "outflow:linear", "outlet");
        settings.SetProcessParameterValue("response_factor", 0.8f);
    } else {
        LogError("The surface runoff option {} is not recognised in Socont.", surfaceRunoff);
        return false;
    }

    settings.AddLoggingToItem("outlet");

    return true;
}

bool GenerateStructureGR4J(SettingsModel& settings, bool discrete) {
    settings.GeneratePrecipitationSplitters(false);
    settings.AddLandCoverBrick("ground", "generic_land_cover");

    if (discrete) {
        LogMessage("Using the discrete version of GR4J (no solver; similar to original).");
    } else {
        LogMessage("Using the solver-based version of GR4J (differs from original version).");
    }

    settings.SelectHydroUnitBrick("ground");
    settings.AddBrickProcess("interception", "interception:gr4j");
    if (discrete) {
        // Ground: interception removes min(P, E); the net precipitation Pn flows to the production store.
        settings.AddBrickProcess("throughfall", "outflow:direct", "production_store");
    } else {
        // Ground: interception removes min(P, E); remainder flows instantaneously to ground_soil
        settings.AddBrickProcess("throughfall", "outflow:rest_direct", "ground_soil");
    }
    settings.SetProcessOutputsAsInstantaneous();

    if (!discrete) {
        // Ground soil (zero capacity): receives Pn, splits into production-store input and direct routing
        settings.AddHydroUnitBrick("ground_soil", "storage");
        settings.AddBrickProcess("production", "infiltration:gr4j", "production_store");
        settings.AddBrickProcess("runoff", "outflow:rest_direct", "uh_input");
    }

    // Production store (capacity X1)
    settings.AddHydroUnitBrick("production_store", "storage");
    settings.AddBrickParameter("capacity", 350.0f);
    if (discrete) {
        // The production process applies the exact discrete GR4J step (infiltration, percolation) and routes
        // PR = (Pn - Ps) + Perc to the unit hydrograph input.
        settings.AddBrickProcess("production", "production:gr4j", "uh_input");
    } else {
        settings.AddBrickProcess("percolation", "percolation:gr4j", "uh_input");
    }
    settings.AddBrickProcess("et", "et:gr4j");
    if (discrete) {
        settings.SetCurrentBrickComputedDirectly();
    }

    // UH input buffer
    settings.AddHydroUnitBrick("uh_input", "storage");
    settings.AddBrickProcess("routing", "routing:gr4j", "outlet");
    if (discrete) {
        settings.SetCurrentBrickComputedDirectly();
    }

    settings.AddLoggingToItem("outlet");

    return true;
}
