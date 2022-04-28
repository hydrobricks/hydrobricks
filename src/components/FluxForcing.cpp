#include "FluxForcing.h"

FluxForcing::FluxForcing()
{}

bool FluxForcing::IsOk() {
    return true;
}

double FluxForcing::GetOutgoingAmount() {
    return m_amount;
}

void FluxForcing::AttachForcing(Forcing* forcing) {
    m_forcing = forcing;
}
