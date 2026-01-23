#include "FluxToAtmosphere.h"

FluxToAtmosphere::FluxToAtmosphere()
    : Flux() {}

bool FluxToAtmosphere::IsValid() const {
    return true;
}

double FluxToAtmosphere::GetAmount() {
    return _amount;
}
