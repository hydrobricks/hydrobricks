#include "FluxToBrickInstantaneous.h"
#include "Brick.h"

FluxToBrickInstantaneous::FluxToBrickInstantaneous(Brick* brick)
    : FluxToBrick(brick)
{}

bool FluxToBrickInstantaneous::IsOk() {
    return true;
}

double FluxToBrickInstantaneous::GetAmount() {
    return 0; // The amount has already been transferred.
}

void FluxToBrickInstantaneous::UpdateFlux(double amount) {
    wxASSERT(m_toBrick);
    m_amount = amount; // Keep it for the logger.
    m_toBrick->GetWaterContainer()->AddAmount(amount);
}