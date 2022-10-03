#include "FluxForcing.h"

FluxForcing::FluxForcing()
    : Flux() {}

bool FluxForcing::IsOk() {
    return true;
}

double FluxForcing::GetAmount() {
    m_amount = m_forcing->GetValue();
    return m_amount;
}

void FluxForcing::AttachForcing(Forcing* forcing) {
    m_forcing = forcing;
}
