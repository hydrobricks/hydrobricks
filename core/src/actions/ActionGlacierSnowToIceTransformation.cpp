#include "ActionGlacierSnowToIceTransformation.h"

#include "Glacier.h"
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
    auto subBasin = _manager->GetSubBasin();

    // Transform snow to ice for each hydro unit.
    for (int id : _hydroUnitIds) {
        HydroUnit* unit = subBasin->GetHydroUnitById(id);

        // Get the glacier brick.
        LandCover* glacierLandCover = unit->GetLandCover(_landCoverName);
        if (glacierLandCover == nullptr || glacierLandCover->GetAreaFraction() == 0) {
            continue;
        }
        Glacier* glacier = dynamic_cast<Glacier*>(glacierLandCover);

        // Get the associated snowpack.
        Brick* snowpack = unit->GetBrick(_landCoverName + "_snowpack");
        if (snowpack == nullptr) {
            wxLogError(_("The brick %s was not found in hydro unit %d"), (_landCoverName + "_snowpack"), id);
            continue;
        }

        // Get snow content.
        double snowWE = snowpack->GetContent("snow");
        if (snowWE <= 0) {
            continue;
        }

        // Transfer to the glacier ice.
        glacier->UpdateContent(glacier->GetContent("ice") + snowWE, "ice");
        snowpack->UpdateContent(0, "snow");
    }

    return true;
}
