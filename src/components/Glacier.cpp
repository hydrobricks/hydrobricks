#include "Glacier.h"

Glacier::Glacier(HydroUnit *hydroUnit)
    : Brick(hydroUnit)
{}

bool Glacier::IsOk() {
    if (m_hydroUnit == nullptr) {
        wxLogError(_("The glacier is not attached to a hydro unit."));
        return false;
    }
    if (m_Inputs.empty()) {
        wxLogError(_("The glacier is not attached to inputs."));
        return false;
    }
    if (m_Outputs.empty()) {
        wxLogError(_("The glacier is not attached to outputs."));
        return false;
    }

    return true;
}

bool Glacier::Compute() {
    return false;
}