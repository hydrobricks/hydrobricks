#include "FluxToOutlet.h"

FluxToOutlet::FluxToOutlet()
    : Flux() {}

bool FluxToOutlet::IsOk() {
    return true;
}

double FluxToOutlet::GetAmount() {
    return _amount;
}
