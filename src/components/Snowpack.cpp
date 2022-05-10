#include "Snowpack.h"

Snowpack::Snowpack(HydroUnit *hydroUnit)
    : Brick(hydroUnit)
{
    m_needsSolver = false;
}

void Snowpack::AssignParameters(const BrickSettings &brickSettings) {
    Brick::AssignParameters(brickSettings);
}

bool Snowpack::IsOk() {
    if (m_hydroUnit == nullptr) {
        wxLogError(_("The snowpack is not attached to a hydro unit."));
        return false;
    }
    if (m_inputs.empty()) {
        wxLogError(_("The snowpack is not attached to inputs."));
        return false;
    }
    for (auto process : m_processes) {
        if (!process->IsOk()) {
            return false;
        }
    }

    return true;
}

vecDouble Snowpack::ComputeOutputs() {
    return {};
}