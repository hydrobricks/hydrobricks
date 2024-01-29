#include "helpers.h"

TimeMachine GenerateTimeMachineDaily() {
    TimeMachine timer;
    timer.Initialize(GetMJD(2020, 1, 1), GetMJD(2020, 1, 31), 1, Day);
    return timer;
}

bool GenerateStructureSocont(SettingsModel& settings, vecStr& landCoverTypes, vecStr& landCoverNames, int soilStorageNb,
                             const string& surfaceRunoff) {
    if (landCoverNames.size() != landCoverTypes.size()) {
        wxLogError(_("The length of the land cover names and types do not match."));
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
            wxLogError(_("The land cover type %s is not used in Socont"), type);
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
            settings.AddBrickParameter("infinite_storage", 1.0);
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
        wxLogMessage(_("Using 2 soil storages."));
        settings.AddHydroUnitBrick("slow_reservoir", "storage");
        settings.AddBrickParameter("capacity", 200.0f);
        settings.AddBrickProcess("et", "et:socont");
        settings.AddBrickProcess("outflow", "outflow:linear", "outlet");
        settings.AddBrickProcess("percolation", "outflow:percolation", "slow_reservoir_2");
        settings.AddBrickProcess("overflow", "overflow", "outlet");
        settings.AddHydroUnitBrick("slow_reservoir_2", "storage");
        settings.AddBrickProcess("outflow", "outflow:linear", "outlet");
        settings.SetProcessParameterValue("response_factor", 0.02f);
    } else {
        wxLogError(_("There can be only one or two groundwater storages."));
    }

    settings.AddHydroUnitBrick("surface_runoff", "storage");
    if (surfaceRunoff == "socont_runoff") {
        settings.AddBrickProcess("runoff", "runoff:socont", "outlet");
    } else if (surfaceRunoff == "linear_storage") {
        wxLogMessage(_("Using a linear storage for the quick flow."));
        settings.AddBrickProcess("outflow", "outflow:linear", "outlet");
        settings.SetProcessParameterValue("response_factor", 0.8f);
    } else {
        wxLogError(_("The surface runoff option %s is not recognised in Socont."), surfaceRunoff);
        return false;
    }

    settings.AddLoggingToItem("outlet");

    return true;
}