#include "Snowpack.h"

Snowpack::Snowpack(HydroUnit *hydroUnit)
    : Brick(hydroUnit)
{}

bool Snowpack::IsOk() {
    if (m_hydroUnit == nullptr) {
        wxLogError(_("The snowpack is not attached to a hydro unit."));
        return false;
    }
    if (m_Inputs.empty()) {
        wxLogError(_("The snowpack is not attached to inputs."));
        return false;
    }
    if (m_Outputs.empty()) {
        wxLogError(_("The snowpack is not attached to outputs."));
        return false;
    }

    return true;
}

bool Snowpack::Compute() {
    return false;
}