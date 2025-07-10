#include "Flux.h"

#include "Modifier.h"

Flux::Flux()
    : _amount(0),
      _changeRate(nullptr),
      _static(false),
      _needsWeighting(false),
      _weight(1.0),
      _fractionLandCover(1.0),
      _fractionTotal(1.0),
      _modifier(nullptr),
      _type("water") {}

void Flux::Reset() {
    _amount = 0;
}

void Flux::UpdateFlux(double amount) {
    if (_fractionTotal < 1.0) {
        _amount = amount * _fractionTotal;
    } else {
        _amount = amount;
    }
}
