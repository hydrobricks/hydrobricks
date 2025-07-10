#include "FluxSimple.h"

FluxSimple::FluxSimple()
    : Flux() {}

bool FluxSimple::IsOk() {
    return true;
}

double FluxSimple::GetAmount() {
    return _amount;
}

void FluxSimple::UpdateFlux(double amount) {
    wxASSERT(_fractionTotal == 1.0);
    _amount = amount;
}
