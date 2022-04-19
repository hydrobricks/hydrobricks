#include "Surface.h"

Surface::Surface(HydroUnit *hydroUnit)
    : Brick(hydroUnit),
      m_waterHeight(0)
{}

bool Surface::IsOk() {
    if (m_hydroUnit == nullptr) return false;
    if (m_Inputs.empty()) return false;
    if (m_Outputs.empty()) return false;

    return true;
}

bool Surface::Compute() {
    return false;
}