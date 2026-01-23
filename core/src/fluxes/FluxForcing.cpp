#include "FluxForcing.h"

FluxForcing::FluxForcing()
    : Flux(),
      _forcing(nullptr) {}

bool FluxForcing::IsValid() const {
    return _forcing != nullptr;
}

double FluxForcing::GetAmount() {
    _amount = _forcing->GetValue();
    return _amount;
}

void FluxForcing::AttachForcing(Forcing* forcing) {
    _forcing = forcing;
}
