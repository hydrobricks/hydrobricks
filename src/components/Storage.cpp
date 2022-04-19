#include "Storage.h"

Storage::Storage(HydroUnit *hydroUnit)
    : Brick(hydroUnit),
      m_capacity(UNDEFINED)
{}

bool Storage::IsOk() {
    if (m_hydroUnit == nullptr) return false;
    if (m_Inputs.empty()) return false;
    if (m_Outputs.empty()) return false;

    return true;
}

bool Storage::Compute() {
    return false;
}