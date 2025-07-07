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
    return _amount;
}

void FluxToBrickInstantaneous::UpdateFlux(double amount) {
    wxASSERT(_toBrick);
    if (_fractionTotal < 1.0) {
        _amount = amount * _fractionTotal;  // Still need to keep it.
    } else {
        _amount = amount;
    }
    _toBrick->GetWaterContainer()->AddAmountToStaticContentChange(_amount);
}
