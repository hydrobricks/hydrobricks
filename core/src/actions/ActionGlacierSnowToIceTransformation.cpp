#include "ActionGlacierSnowToIceTransformation.h"

#include "GlobVars.h"
#include "HydroUnit.h"
#include "ModelHydro.h"

ActionGlacierSnowToIceTransformation::ActionGlacierSnowToIceTransformation(int month, int day,
                                                                           const string& landCoverName)
    : Action() {
    AddRecursiveDate(month, day);
    _landCoverName = landCoverName;
}

bool ActionGlacierSnowToIceTransformation::Init() {
    // Loop over all hydro units in the sub-basin and register those with the specified land cover.
    _hydroUnitIds.clear();
    for (auto unit : _manager->GetSubBasin()->GetHydroUnits()) {
        if (unit->GetLandCover(_landCoverName) != nullptr) {
            _hydroUnitIds.push_back(unit->GetId());
        }
    }

    return true;
}

void ActionGlacierSnowToIceTransformation::Reset() {
    // Nothing to reset.
}

bool ActionGlacierSnowToIceTransformation::Apply(double) {
    // Transform snow to ice for each hydro unit.
    for (int i = 0; i < _hydroUnitIds.size(); ++i) {
        int id = _hydroUnitIds[i];
        HydroUnit* unit = subBasin->GetHydroUnitById(id);

        // Get the glacier brick.
        LandCover* brick = unit->GetLandCover(_landCoverName);
        if (brick == nullptr || brick->GetAreaFraction() == 0) {
            continue;
        }

        // Get the associated snowpack.
        LandCover* snowpack = unit->GetLandCover(_landCoverName + "_snowpack");
        if (snowpack == nullptr) {
            wxLogError(_("The snowpack land cover %s was not found in hydro unit %d"), (_landCoverName + "_snowpack"), id);
            continue;
        }

        // Get snow content.
        double snowWE = snowpack->GetContent("snow");
        if (snowWE <= 0) {
            continue;
        }

        // Transfer to the glacier ice.
        brick->UpdateContent(brick->GetContent("ice") + snowWE, "ice");
        snowpack->UpdateContent(0, "snow");
    }

    return true;
}
