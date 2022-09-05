#include "FluxToAtmosphere.h"

FluxToAtmosphere::FluxToAtmosphere()
    : Flux()
{}

bool FluxToAtmosphere::IsOk() {
    return true;
}

double FluxToAtmosphere::GetAmount() {
    return m_amount;
}