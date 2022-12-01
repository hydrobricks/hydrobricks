#include "FluxToBrickInstantaneous.h"

#include "Brick.h"

FluxToBrickInstantaneous::FluxToBrickInstantaneous(Brick* brick)
    : FluxToBrick(brick) {}

bool FluxToBrickInstantaneous::IsOk() {
    return true;
}

double FluxToBrickInstantaneous::GetAmount() {
    return 0;  // The amount has already been transferred.
}

double FluxToBrickInstantaneous::GetRealAmount() {
    return m_amount;
}

void FluxToBrickInstantaneous::UpdateFlux(double amount) {
    wxASSERT(m_toBrick);
    if (m_fraction < 1.0) {
        m_amount = amount * m_fraction;  // Still need to keep it.
    } else {
        m_amount = amount;
    }
    m_toBrick->GetWaterContainer()->AddAmountToStaticContentChange(m_amount);
}
