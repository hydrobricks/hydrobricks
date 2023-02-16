#include "FluxSimple.h"

FluxSimple::FluxSimple()
    : Flux() {}

bool FluxSimple::IsOk() {
    return true;
}

double FluxSimple::GetAmount() {
    return m_amount;
}

void FluxSimple::UpdateFlux(double amount) {
    wxASSERT(m_fractionTotal == 1.0);
    m_amount = amount;
}
