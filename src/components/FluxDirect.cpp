#include "FluxDirect.h"

FluxDirect::FluxDirect()
{}

bool FluxDirect::IsOk() {
    if (m_in == nullptr) return false;
    if (m_out == nullptr) return false;

    return true;
}

double FluxDirect::GetWaterAmount() {
    return m_waterAmount;
}