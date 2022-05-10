#include "Glacier.h"

Glacier::Glacier(HydroUnit *hydroUnit)
    : Brick(hydroUnit)
{
    m_needsSolver = false;
}

void Glacier::AssignParameters(const BrickSettings &brickSettings) {
    Brick::AssignParameters(brickSettings);
}

bool Glacier::IsOk() {
    if (m_hydroUnit == nullptr) {
        wxLogError(_("The glacier is not attached to a hydro unit."));
        return false;
    }
    if (m_inputs.empty()) {
        wxLogError(_("The glacier is not attached to inputs."));
        return false;
    }
    for (auto process : m_processes) {
        if (!process->IsOk()) {
            return false;
        }
    }

    return true;
}

vecDouble Glacier::ComputeOutputs() {
    return {};
}