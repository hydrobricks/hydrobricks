#include "SplitterSnowRainLinear.h"

SplitterSnowRainLinear::SplitterSnowRainLinear()
    : Splitter(),
      _precipitation(nullptr),
      _temperature(nullptr),
      _transitionStart(nullptr),
      _transitionEnd(nullptr),
      _rainCorrectionFactor(nullptr),
      _snowCorrectionFactor(nullptr) {}

bool SplitterSnowRainLinear::IsValid() const {
    if (_outputs.size() != 2) {
        LogError("SplitterSnowRainLinear should have 2 outputs.");
        return false;
    }

    return true;
}

void SplitterSnowRainLinear::SetParameters(const SplitterSettings& splitterSettings) {
    _transitionStart = GetParameterValuePointer(splitterSettings, "transition_start");
    _transitionEnd = GetParameterValuePointer(splitterSettings, "transition_end");
    _rainCorrectionFactor = GetParameterValuePointerOrUnit(splitterSettings, "rain_correction_factor");
    _snowCorrectionFactor = GetParameterValuePointerOrUnit(splitterSettings, "snow_correction_factor");
}

void SplitterSnowRainLinear::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == VariableType::Precipitation) {
        _precipitation = forcing;
    } else if (forcing->GetType() == VariableType::Temperature) {
        _temperature = forcing;
    } else {
        throw ModelConfigError("Forcing must be of type Temperature or Precipitation");
    }
}

double* SplitterSnowRainLinear::GetValuePointer(const string& name) {
    if (name == "rain") {
        return _outputs[0]->GetAmountPointer();
    }
    if (name == "snow") {
        return _outputs[1]->GetAmountPointer();
    }

    return nullptr;
}

void SplitterSnowRainLinear::Compute() {
    double precip = _precipitation->GetValue();
    double rcf = *_rainCorrectionFactor;
    double scf = *_snowCorrectionFactor;
    if (_temperature->GetValue() <= *_transitionStart) {
        _outputs[0]->UpdateFlux(0);
        _outputs[1]->UpdateFlux(precip * scf);
    } else if (_temperature->GetValue() >= *_transitionEnd) {
        _outputs[0]->UpdateFlux(precip * rcf);
        _outputs[1]->UpdateFlux(0);
    } else {
        double rainFraction = (_temperature->GetValue() - *_transitionStart) / (*_transitionEnd - *_transitionStart);
        _outputs[0]->UpdateFlux(precip * rainFraction * rcf);
        _outputs[1]->UpdateFlux(precip * (1 - rainFraction) * scf);
    }
}
