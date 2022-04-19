#include "Snowpack.h"

Snowpack::Snowpack(HydroUnit *hydroUnit)
    : Brick(hydroUnit)
{}

bool Snowpack::IsOk() {
    if (m_hydroUnit == nullptr) return false;
    if (m_Inputs.empty()) return false;
    if (m_Outputs.empty()) return false;

    return true;
}

bool Snowpack::Compute() {
    return false;
}