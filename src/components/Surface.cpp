#include "Surface.h"

Surface::Surface(HydroUnit *hydroUnit)
    : Brick(hydroUnit),
      m_waterHeight(0)
{}

bool Surface::IsOk() {
    if (m_hydroUnit == nullptr) {
        wxLogError(_("The surface is not attached to a hydro unit."));
        return false;
    }
    if (m_inputs.empty()) {
        wxLogError(_("The surface is not attached to inputs."));
        return false;
    }
    if (m_outputs.empty()) {
        wxLogError(_("The surface is not attached to outputs."));
        return false;
    }

    return true;
}

bool Surface::Compute() {
    return false;
}