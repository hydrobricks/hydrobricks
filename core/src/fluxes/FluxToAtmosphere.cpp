#include "FluxToAtmosphere.h"

FluxToAtmosphere::FluxToAtmosphere()
    : Flux() {}

bool FluxToAtmosphere::IsOk() const {
    return true;
}

double FluxToAtmosphere::GetAmount() {
    return _amount;
}
