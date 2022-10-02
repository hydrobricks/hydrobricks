#include "FluxToBrick.h"

#include "Brick.h"

FluxToBrick::FluxToBrick(Brick* brick) : Flux(), m_toBrick(brick) {}

bool FluxToBrick::IsOk() {
    return true;
}

double FluxToBrick::GetAmount() {
    return m_amount;
}

void FluxToBrick::UpdateFlux(double amount) {
    wxASSERT(m_toBrick);
    if (m_fraction < 1.0) {
        m_amount = amount * m_fraction;
    } else {
        m_amount = amount;
    }
}
