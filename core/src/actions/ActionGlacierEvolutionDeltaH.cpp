#include "ActionGlacierEvolutionDeltaH.h"

#include "GlobVars.h"
#include "HydroUnit.h"
#include "ModelHydro.h"

ActionGlacierEvolutionDeltaH::ActionGlacierEvolutionDeltaH() = default;

void ActionGlacierEvolutionDeltaH::AddLookupTables(int month, const string& landCoverName, const axi& hydroUnitIds,
                                                   const axxd& areas, const axxd& volumes) {
    _recursive = true;
    _recursiveMonths.push_back(month);
    _recursiveDays.push_back(1);
    _landCoverName = landCoverName;
    _hydroUnitIds = hydroUnitIds;
    _tableArea = areas;
    _tableVolume = volumes;

    double initialVolume = _tableVolume.row(0).sum();
    _initialGlacierWE = initialVolume * g_iceDensity;  // Convert to mm w.e.
}

bool ActionGlacierEvolutionDeltaH::Init() {
    for (int i = 0; i < _hydroUnitIds.size(); i++) {
        int id = _hydroUnitIds[i];
        HydroUnit* unit = _manager->GetHydroUnitById(id);
        if (unit == nullptr) {
            wxLogError(_("The hydro unit %d was not found"), id);
            return false;
        }

        // Check that the glacier area corresponds to the lookup table initial area.
        double areaRef = _tableArea(0, i);
        double areaInModel = unit->GetLandCover(_landCoverName)->GetAreaFraction() * unit->GetArea();
        if (areaInModel == 0) {
            // If the glacier area is zero, initialize the fraction.
            double fraction = areaRef / unit->GetArea();
            assert(fraction >= 0 && fraction <= 1);
            unit->ChangeLandCoverAreaFraction(_landCoverName, fraction);
        } else if (areaInModel != areaRef) {
            wxLogError(_("The glacier area fraction in hydro unit %d does not match the lookup table initial area."),
                       id);
            return false;
        }

        // Initialize the glacier container.
        LandCover* brick = unit->GetLandCover(_landCoverName);
        if (brick == nullptr) {
            wxLogError(_("The land cover %s was not found in hydro unit %d"), _landCoverName, id);
            return false;
        }
        double iceWE = _tableVolume(0, i) * g_iceDensity / areaRef;
        brick->UpdateContent(iceWE, "ice");
        brick->SetInitialState(iceWE, "ice");
    }

    return true;
}

bool ActionGlacierEvolutionDeltaH::Apply(double) {
    // Get the list of hydro units.
    auto subBasin = _manager->GetModel()->GetSubBasin();

    // Compute the total glacier (_landCoverName) water equivalent (w.e.).
    double glacierWE = 0.0;
    for (int i = 0; i < _hydroUnitIds.size(); ++i) {
        int id = _hydroUnitIds[i];
        HydroUnit* unit = subBasin->GetHydroUnitById(id);
        LandCover* brick = unit->GetLandCover(_landCoverName);
        if (brick == nullptr || brick->GetAreaFraction() == 0) {
            continue;
        }
        double area = brick->GetAreaFraction() * unit->GetArea();
        double brickGlacierWE = brick->GetContent("ice");

        glacierWE += area * brickGlacierWE;
    }

    // Get the percentage of glacier retreat.
    double glacierRetreatPc = (_initialGlacierWE - glacierWE) / _initialGlacierWE;

    // Get the percentage of glacier retreat for each row of the table.
    int nRows = _tableArea.rows();
    double rowPcIncrement = 1.0 / (nRows - 1);

    // Get corresponding row in the lookup table.
    int row = static_cast<int>(glacierRetreatPc / rowPcIncrement);

    // Update the glacier area for each hydro unit if the row has changed.
    if (row != _lastRow) {
        for (int i = 0; i < _hydroUnitIds.size(); ++i) {
            int id = _hydroUnitIds[i];
            HydroUnit* unit = subBasin->GetHydroUnitById(id);
            double fraction = _tableArea(row, i) / unit->GetArea();
            assert(fraction >= 0 && fraction <= 1);
            unit->ChangeLandCoverAreaFraction(_landCoverName, fraction);
        }
        _lastRow = row;
    }

    // Update the glacier water equivalent for each hydro unit to spread the glacier ice accordingly.
    double rowVolumeSum = _tableVolume.row(row).sum();
    for (int i = 0; i < _hydroUnitIds.size(); ++i) {
        int id = _hydroUnitIds[i];
        HydroUnit* unit = subBasin->GetHydroUnitById(id);
        LandCover* brick = unit->GetLandCover(_landCoverName);
        double areaGlacier = _tableArea(row, i);
        if (areaGlacier == 0) {
            brick->UpdateContent(0, "ice");
            assert(_tableVolume(row, i) == 0);
        } else {
            double volumeRatio = _tableVolume(row, i) / rowVolumeSum;
            double iceWE = glacierWE * volumeRatio / areaGlacier;
            brick->UpdateContent(iceWE, "ice");
        }
    }

    return true;
}
