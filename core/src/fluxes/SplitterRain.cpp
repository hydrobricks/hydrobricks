#include "SplitterRain.h"

SplitterRain::SplitterRain()
    : Splitter(),
      _precipitation(nullptr) {}

bool SplitterRain::IsOk() {
    if (_outputs.size() != 1) {
        wxLogError(_("SplitterRain should have 1 output."));
        return false;
    }

    return true;
}

void SplitterRain::SetParameters(const SplitterSettings&) {
    //
}

void SplitterRain::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == Precipitation) {
        _precipitation = forcing;
    } else {
        throw InvalidArgument("Forcing must be of type Precipitation");
    }
}

double* SplitterRain::GetValuePointer(const string& name) {
    if (name == "rain") {
        return _outputs[0]->GetAmountPointer();
    }

    return nullptr;
}

void SplitterRain::Compute() {
    _outputs[0]->UpdateFlux(_precipitation->GetValue());
}
