#include "SplitterSnowRainThreshold.h"

SplitterSnowRainThreshold::SplitterSnowRainThreshold()
    : Splitter(),
      _precipitation(nullptr),
      _temperature(nullptr),
      _threshold(nullptr),
      _rainCorrectionFactor(nullptr),
      _snowCorrectionFactor(nullptr) {}

bool SplitterSnowRainThreshold::IsValid() const {
    if (_outputs.size() != 2) {
        LogError("SplitterSnowRainThreshold should have 2 outputs.");
        return false;
    }
    return true;
}

void SplitterSnowRainThreshold::SetParameters(const SplitterSettings& splitterSettings) {
    _threshold = GetParameterValuePointer(splitterSettings, "threshold");
    _rainCorrectionFactor = GetParameterValuePointerOrUnit(splitterSettings, "rain_correction_factor");
    _snowCorrectionFactor = GetParameterValuePointerOrUnit(splitterSettings, "snow_correction_factor");
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
    double precip = _precipitation->GetValue();
    if (_temperature->GetValue() <= *_threshold) {
        _outputs[0]->UpdateFlux(0);
        _outputs[1]->UpdateFlux(precip * *_snowCorrectionFactor);
    } else {
        _outputs[0]->UpdateFlux(precip * *_rainCorrectionFactor);
        _outputs[1]->UpdateFlux(0);
    }
}
