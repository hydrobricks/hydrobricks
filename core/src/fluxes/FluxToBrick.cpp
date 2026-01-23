#include "FluxToBrick.h"

#include "Brick.h"

FluxToBrick::FluxToBrick(Brick* brick)
    : Flux(),
      _toBrick(brick) {}

bool FluxToBrick::IsValid() const {
    return _toBrick != nullptr;
}

double FluxToBrick::GetAmount() {
    return _amount;
}

void FluxToBrick::UpdateFlux(double amount) {
    wxASSERT(_toBrick);
    if (_fractionTotal < 1.0) {
        _amount = amount * _fractionTotal;
    } else {
        _amount = amount;
    }
}
