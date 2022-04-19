#include "Glacier.h"

Glacier::Glacier(HydroUnit *hydroUnit)
    : Brick(hydroUnit)
{}

bool Glacier::IsOk() {
    if (m_hydroUnit == nullptr) return false;
    if (m_Inputs.empty()) return false;
    if (m_Outputs.empty()) return false;

    return true;
}

bool Glacier::Compute() {
    return false;
}