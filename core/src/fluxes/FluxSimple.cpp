#include "FluxSimple.h"

FluxSimple::FluxSimple()
    : Flux() {}

bool FluxSimple::IsValid() const {
    return true;
}

double FluxSimple::GetAmount() {
    return _amount;
}

void FluxSimple::UpdateFlux(double amount) {
    assert(_fractionTotal == 1.0);
    _amount = amount;
}
