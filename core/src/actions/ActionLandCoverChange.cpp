#include "ActionLandCoverChange.h"

#include "HydroUnit.h"
#include "ModelHydro.h"

ActionLandCoverChange::ActionLandCoverChange() = default;

void ActionLandCoverChange::AddChange(double date, int hydroUnitId, const string& landCoverName, double area) {
    int landCoverId = GetLandCoverId(landCoverName);

    int index = GetIndexForInsertion(date);

    _sporadicDates.insert(_sporadicDates.begin() + index, date);
    _hydroUnitIds.insert(_hydroUnitIds.begin() + index, hydroUnitId);
    _landCoverIds.insert(_landCoverIds.begin() + index, landCoverId);
    _areas.insert(_areas.begin() + index, area);
}

bool ActionLandCoverChange::Apply(double) {
    wxASSERT(_sporadicDates.size() > _cursor);
    wxASSERT(_hydroUnitIds.size() > _cursor);
    wxASSERT(_areas.size() > _cursor);
    wxASSERT(_landCoverIds.size() > _cursor);
    wxASSERT(_landCoverNames.size() > _landCoverIds[_cursor]);

    HydroUnit* unit = _manager->GetHydroUnitById(_hydroUnitIds[_cursor]);
    string landCoverName = _landCoverNames[_landCoverIds[_cursor]];
    double areaFraction = _areas[_cursor] / unit->GetArea();

    return unit->ChangeLandCoverAreaFraction(landCoverName, areaFraction);
}

int ActionLandCoverChange::GetLandCoverId(const string& landCoverName) {
    int landCoverId = -1;
    for (int i = 0; i < _landCoverNames.size(); ++i) {
        if (_landCoverNames[i] == landCoverName) {
            landCoverId = i;
            break;
        }
    }

    // Insert as a new land cover if not found.
    if (landCoverId == -1) {
        _landCoverNames.push_back(landCoverName);
        landCoverId = static_cast<int>(_landCoverNames.size()) - 1;
    }

    return landCoverId;
}