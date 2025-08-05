#include "SplitterSnowRain.h"

SplitterSnowRain::SplitterSnowRain()
    : Splitter(),
      _precipitation(nullptr),
      _temperature(nullptr),
      _transitionStart(nullptr),
      _transitionEnd(nullptr) {}

bool SplitterSnowRain::IsOk() {
    if (_outputs.size() != 2) {
        wxLogError(_("SplitterSnowRain should have 2 outputs."));
        return false;
    }

    return true;
}

void SplitterSnowRain::SetParameters(const SplitterSettings& splitterSettings) {
    _transitionStart = GetParameterValuePointer(splitterSettings, "transition_start");
    _transitionEnd = GetParameterValuePointer(splitterSettings, "transition_end");
}

void SplitterSnowRain::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == Precipitation) {
        _precipitation = forcing;
    } else if (forcing->GetType() == Temperature) {
        _temperature = forcing;
    } else {
        throw InvalidArgument("Forcing must be of type Temperature or Precipitation");
    }
}

double* SplitterSnowRain::GetValuePointer(const string& name) {
    if (name == "rain") {
        return _outputs[0]->GetAmountPointer();
    }
    if (name == "snow") {
        return _outputs[1]->GetAmountPointer();
    }

    return nullptr;
}

void SplitterSnowRain::Compute() {
    if (_temperature->GetValue() <= *_transitionStart) {
        _outputs[0]->UpdateFlux(0);
        _outputs[1]->UpdateFlux(_precipitation->GetValue());
    } else if (_temperature->GetValue() >= *_transitionEnd) {
        _outputs[0]->UpdateFlux(_precipitation->GetValue());
        _outputs[1]->UpdateFlux(0);
    } else {
        double rainFraction = (_temperature->GetValue() - *_transitionStart) / (*_transitionEnd - *_transitionStart);
        _outputs[0]->UpdateFlux(_precipitation->GetValue() * rainFraction);
        _outputs[1]->UpdateFlux(_precipitation->GetValue() * (1 - rainFraction));
    }
}
