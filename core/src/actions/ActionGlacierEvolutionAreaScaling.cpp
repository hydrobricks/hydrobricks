#include "ActionGlacierEvolutionAreaScaling.h"

#include "GlobVars.h"
#include "HydroUnit.h"
#include "ModelHydro.h"

ActionGlacierEvolutionAreaScaling::ActionGlacierEvolutionAreaScaling() = default;

void ActionGlacierEvolutionAreaScaling::AddLookupTables(int month, const string& landCoverName, const axi& hydroUnitIds,
                                                        const axxd& areas, const axxd& volumes) {
    _recursive = true;
    _recursiveMonths.push_back(month);
    _recursiveDays.push_back(1);
    _landCoverName = landCoverName;
    _hydroUnitIds = hydroUnitIds;
    _tableArea = areas;
    _tableVolume = volumes;
    _initialGlacierWE = _tableVolume.row(0) * constants::iceDensity;  // Convert to mm w.e.
}

bool ActionGlacierEvolutionAreaScaling::Init() {
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
        } else if (std::abs(areaInModel - areaRef) > PRECISION) {
            wxLogError(_("The glacier area fraction in hydro unit %d does not match the lookup table "
                         "initial area (%g vs %g)."),
                       id, areaInModel, areaRef);
            return false;
        }

        // Initialize the glacier container.
        LandCover* brick = unit->GetLandCover(_landCoverName);
        if (brick == nullptr) {
            wxLogError(_("The land cover %s was not found in hydro unit %d"), _landCoverName, id);
            return false;
        }
        double iceWE = _tableVolume(0, i) * constants::iceDensity / areaRef;
        brick->UpdateContent(iceWE, "ice");
        brick->SetInitialState(iceWE, "ice");
    }

    return true;
}

void ActionGlacierEvolutionAreaScaling::Reset() {
    // Set the land cover area fraction to 0 for all hydro units.
    for (int id : _hydroUnitIds) {
        HydroUnit* unit = _manager->GetHydroUnitById(id);
        unit->ChangeLandCoverAreaFraction(_landCoverName, 0);
    }

    // Re-initialize.
    Init();
}

bool ActionGlacierEvolutionAreaScaling::Apply(double) {
    // Get the list of hydro units.
    auto subBasin = _manager->GetModel()->GetSubBasin();

    // Get the percentage of glacier retreat for each row of the table.
    int nRows = _tableArea.rows();
    double rowPcIncrement = 1.0 / (nRows - 1);

    // Change the glacier area for each hydro unit based on the lookup table.
    for (int i = 0; i < _hydroUnitIds.size(); ++i) {
        int id = _hydroUnitIds[i];
        HydroUnit* unit = subBasin->GetHydroUnitById(id);
        LandCover* brick = unit->GetLandCover(_landCoverName);
        if (brick == nullptr || brick->GetAreaFraction() == 0) {
            continue;
        }
        double area = brick->GetAreaFraction() * unit->GetArea();
        double brickIceWE = brick->GetContent("ice");
        double iceVolume = area * brickIceWE;

        if (iceVolume == 0) {
            // If the glacier water equivalent is zero, set the area to zero.
            unit->ChangeLandCoverAreaFraction(_landCoverName, 0);
            continue;
        }

        // Get the percentage of glacier retreat for the current hydro unit.
        double glacierRetreatPc = (_initialGlacierWE[i] - iceVolume) / _initialGlacierWE[i];

        // Get corresponding row in the lookup table.
        int row = static_cast<int>(glacierRetreatPc / rowPcIncrement);

        // Update the glacier area.
        double newFraction = _tableArea(row, i) / unit->GetArea();
        assert(newFraction >= 0 && newFraction <= 1);
        unit->ChangeLandCoverAreaFraction(_landCoverName, newFraction);

        // Update the glacier water equivalent to match the new area.
        double newArea = _tableArea(row, i);
        double iceWE = iceVolume / newArea;  // Convert glacier water equivalent to mm w.e.
        brick->UpdateContent(iceWE, "ice");
    }

    return true;
}
