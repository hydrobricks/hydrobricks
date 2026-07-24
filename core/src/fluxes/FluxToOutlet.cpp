#include "FluxToOutlet.h"

FluxToOutlet::FluxToOutlet()
    : Flux() {}

bool FluxToOutlet::IsValid() const {
    return true;
}

double FluxToOutlet::GetAmount() {
    return _amount;
}
