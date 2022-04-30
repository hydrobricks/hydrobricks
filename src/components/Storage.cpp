#include "Storage.h"

Storage::Storage(HydroUnit *hydroUnit)
    : Brick(hydroUnit)
{
    m_needsSolver = false;
}

void Storage::AssignParameters(const BrickSettings &brickSettings) {
    Brick::AssignParameters(brickSettings);
}

bool Storage::IsOk() {
    if (m_hydroUnit == nullptr) {
        wxLogError(_("The storage is not attached to a hydro unit."));
        return false;
    }
    if (m_inputs.empty()) {
        wxLogError(_("The storage is not attached to inputs."));
        return false;
    }
    if (m_outputs.empty()) {
        wxLogError(_("The storage is not attached to outputs."));
        return false;
    }

    return true;
}

vecDouble Storage::ComputeOutputs() {
    return {};
}
