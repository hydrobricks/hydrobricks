#include "FluxToBrickInstantaneous.h"

#include "Brick.h"
#include "Snowpack.h"

FluxToBrickInstantaneous::FluxToBrickInstantaneous(Brick* brick)
    : FluxToBrick(brick) {}

bool FluxToBrickInstantaneous::IsValid() const {
    return true;
}

double FluxToBrickInstantaneous::GetAmount() {
    return 0;  // The amount has already been transferred.
}

double FluxToBrickInstantaneous::GetRealAmount() const {
    return _amount;
}

void FluxToBrickInstantaneous::UpdateFlux(double amount) {
    assert(_toBrick);
    if (_fractionTotal != 1.0) {
        _amount = amount * _fractionTotal;  // Still need to keep it.
    } else {
        _amount = amount;
    }

    if (_type == ContentType::Water) {
        _toBrick->GetWaterContainer()->AddAmountToStaticContentChange(_amount);
    } else if (_type == ContentType::Snow) {
        auto snowBrick = dynamic_cast<Snowpack*>(_toBrick);
        assert(snowBrick);
        snowBrick->GetSnowContainer()->AddAmountToStaticContentChange(_amount);
    } else {
        throw ModelConfigError(std::format("The content type '{}' is not supported.", ContentTypeToString(_type)));
    }
}
