#include "SplitterSnowRainThreshold.h"

SplitterSnowRainThreshold::SplitterSnowRainThreshold()
    : Splitter(),
      _precipitation(nullptr),
      _temperature(nullptr),
      _threshold(nullptr) {}

bool SplitterSnowRainThreshold::IsValid() const {
    if (_outputs.size() != 2) {
        LogError("SplitterSnowRainThreshold should have 2 outputs.");
        return false;
    }
    return true;
}

void SplitterSnowRainThreshold::SetParameters(const SplitterSettings& splitterSettings) {
    _threshold = GetParameterValuePointer(splitterSettings, "threshold");
}

void SplitterSnowRainThreshold::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == VariableType::Precipitation) {
        _precipitation = forcing;
    } else if (forcing->GetType() == VariableType::Temperature) {
        _temperature = forcing;
    } else {
        throw ModelConfigError("Forcing must be of type Temperature or Precipitation");
    }
}

double* SplitterSnowRainThreshold::GetValuePointer(const string& name) {
    if (name == "rain") {
        return _outputs[0]->GetAmountPointer();
    }
    if (name == "snow") {
        return _outputs[1]->GetAmountPointer();
    }
    return nullptr;
}

void SplitterSnowRainThreshold::Compute() {
    if (_temperature->GetValue() <= *_threshold) {
        _outputs[0]->UpdateFlux(0);
        _outputs[1]->UpdateFlux(_precipitation->GetValue());
    } else {
        _outputs[0]->UpdateFlux(_precipitation->GetValue());
        _outputs[1]->UpdateFlux(0);
    }
}
