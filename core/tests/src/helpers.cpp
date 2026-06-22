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
    settings.AddBrickProcess("runoff", "outflow:rest", "surface_runoff");
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
        settings.AddBrickProcess("throughfall", "outflow:rest", "production_store");
    } else {
        // Ground: interception removes min(P, E); remainder flows instantaneously to ground_soil
        settings.AddBrickProcess("throughfall", "outflow:rest", "ground_soil");
    }
    settings.SetProcessOutputsAsInstantaneous();

    if (!discrete) {
        // Ground soil (zero capacity): receives Pn, splits into production-store input (Ps) and the
        // direct routing branch (Pn - Ps). outflow:rest takes what infiltration leaves; it must be
        // declared after the infiltration process.
        settings.AddHydroUnitBrick("ground_soil", "storage");
        settings.AddBrickProcess("production", "infiltration:gr4j", "production_store");
        settings.AddBrickProcess("runoff", "outflow:rest", "uh_input");
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

bool GenerateStructureGR6J(SettingsModel& settings, bool discrete) {
    // GR6J shares the GR4J production store, interception, throughfall and ET; only the routing
    // process differs (routing:gr6j adds the threshold exchange and the exponential store).
    settings.GeneratePrecipitationSplitters(false);
    settings.AddLandCoverBrick("ground", "generic_land_cover");

    settings.SelectHydroUnitBrick("ground");
    settings.AddBrickProcess("interception", "interception:gr4j");
    if (discrete) {
        settings.AddBrickProcess("throughfall", "outflow:rest", "production_store");
    } else {
        settings.AddBrickProcess("throughfall", "outflow:rest", "ground_soil");
    }
    settings.SetProcessOutputsAsInstantaneous();

    if (!discrete) {
        settings.AddHydroUnitBrick("ground_soil", "storage");
        settings.AddBrickProcess("production", "infiltration:gr4j", "production_store");
        settings.AddBrickProcess("runoff", "outflow:rest", "uh_input");
    }

    // Production store (capacity X1)
    settings.AddHydroUnitBrick("production_store", "storage");
    settings.AddBrickParameter("capacity", 350.0f);
    if (discrete) {
        settings.AddBrickProcess("production", "production:gr4j", "uh_input");
    } else {
        settings.AddBrickProcess("percolation", "percolation:gr4j", "uh_input");
    }
    settings.AddBrickProcess("et", "et:gr4j");
    if (discrete) {
        settings.SetCurrentBrickComputedDirectly();
    }

    // UH input buffer routed by the GR6J routing process
    settings.AddHydroUnitBrick("uh_input", "storage");
    settings.AddBrickProcess("routing", "routing:gr6j", "outlet");
    if (discrete) {
        settings.SetCurrentBrickComputedDirectly();
    }

    settings.AddLoggingToItem("outlet");

    return true;
}

bool GenerateStructureHBV96(SettingsModel& settings, bool withSnow, bool rainToSnowpack) {
    // Precipitation (linear rain/snow transition, as the HBV-96 interval TT ± TTI/2)
    settings.GeneratePrecipitationSplitters(withSnow);
    settings.AddLandCoverBrick("ground", "generic_land_cover");

    if (withSnow) {
        // Snowpacks with liquid water retention (CWH) and refreezing (CFR). As in the
        // original HBV snow routine, the rain falls on the snowpack: it is retained in
        // the liquid water storage (and can refreeze) and only the excess over the
        // holding capacity reaches the ground.
        settings.GenerateSnowpacksWithWaterRetention("melt:degree_day", "outflow:snow_holding", rainToSnowpack);
        settings.AddSnowpackRefreezing("refreeze:degree_day");
    }

    // Beta-function split between the soil moisture and the upper zone. The recharge
    // (outflow:rest) is the complement of the infiltration and must be declared after it.
    settings.SelectHydroUnitBrick("ground");
    settings.AddBrickProcess("infiltration", "infiltration:hbv", "soil_moisture");
    settings.AddBrickProcess("recharge", "outflow:rest", "upper_zone");
    settings.SetProcessOutputsAsStatic();

    // The brick declaration order matters: the solver applies the bricks in order, so
    // every brick-to-brick flux must flow towards a later brick to be received within
    // the same iteration (hence the soil moisture is declared after the upper zone,
    // which feeds it through the capillary transport).

    // Upper zone: capillary transport, constant percolation and non-linear runoff
    settings.AddHydroUnitBrick("upper_zone", "storage");
    settings.AddBrickProcess("capillary", "capillary:hbv", "soil_moisture");
    settings.AddBrickProcess("percolation", "percolation:constant", "lower_zone");
    settings.AddBrickProcess("runoff", "runoff:hbv", "routing");

    // Lower zone: linear reservoir
    settings.AddHydroUnitBrick("lower_zone", "storage");
    settings.AddBrickProcess("outflow", "outflow:linear", "routing");

    // Soil moisture storage (capacity FC). The overflow is a numerical safety only
    // (the infiltration and the capillary flux both vanish at FC).
    settings.AddHydroUnitBrick("soil_moisture", "storage");
    settings.AddBrickParameter("capacity", 250.0f);
    settings.AddBrickProcess("et", "et:hbv");
    settings.AddBrickProcess("overflow", "overflow", "routing");

    // Transformation function: triangular unit hydrograph (MAXBAS)
    settings.AddHydroUnitBrick("routing", "storage");
    settings.AddBrickProcess("routing", "routing:hbv", "outlet");

    settings.AddLoggingToItem("outlet");

    return true;
}
