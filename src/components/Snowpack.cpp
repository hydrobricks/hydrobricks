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
    if (m_outputs.empty()) {
        wxLogError(_("The snowpack is not attached to outputs."));
        return false;
    }

    return true;
}

std::vector<double> Snowpack::ComputeOutputs() {
    return {};
}