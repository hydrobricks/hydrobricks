#include "FluxToOutlet.h"

FluxToOutlet::FluxToOutlet()
    : Flux() {}

bool FluxToOutlet::IsOk() const {
    return true;
}

double FluxToOutlet::GetAmount() {
    return _amount;
}
