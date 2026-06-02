#include "ProcessMeltCemaNeige.h"

#include <cmath>

#include "Brick.h"
#include "WaterContainer.h"

ProcessMeltCemaNeige::ProcessMeltCemaNeige(WaterContainer* container)
    : ProcessMelt(container),
      _temperature(nullptr),
      _degreeDayFactor(nullptr),
      _coldContentFactor(nullptr),
      _meltingTemp(nullptr),
      _meanAnnualSnow(nullptr),
      _coldContent(0.0) {}

void ProcessMeltCemaNeige::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("degree_day_factor", 3.0f);
    modelSettings->AddProcessParameter("cold_content_factor", 0.0f);
    modelSettings->AddProcessParameter("melting_temperature", 0.0f);
    modelSettings->AddProcessParameter("mean_annual_snow", 100.0f);
    modelSettings->AddProcessForcing("temperature");
}

bool ProcessMeltCemaNeige::IsValid() const {
    if (!ProcessMelt::IsValid()) {
        return false;
    }
    if (_temperature == nullptr) {
        LogError("CemaNeige process requires temperature forcing.");
        return false;
    }
    if (_degreeDayFactor == nullptr || _coldContentFactor == nullptr || _meltingTemp == nullptr ||
        _meanAnnualSnow == nullptr) {
        LogError("CemaNeige process: one or more parameters are missing.");
        return false;
    }

    return true;
}

void ProcessMeltCemaNeige::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _degreeDayFactor = GetParameterValuePointer(processSettings, "degree_day_factor");
    _coldContentFactor = GetParameterValuePointer(processSettings, "cold_content_factor");
    _meltingTemp = GetParameterValuePointer(processSettings, "melting_temperature");
    _meanAnnualSnow = GetParameterValuePointer(processSettings, "mean_annual_snow");
}

void ProcessMeltCemaNeige::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == VariableType::Temperature) {
        _temperature = forcing;
    } else {
        throw ModelConfigError("CemaNeige: forcing must be of type Temperature.");
    }
}

void ProcessMeltCemaNeige::Reset() {
    ProcessMelt::Reset();
    _coldContent = 0.0;
}

vecDouble ProcessMeltCemaNeige::GetRates() {
    if (!_container->ContentAccessible()) {
        return {0};
    }

    double T = _temperature->GetValue();
    double Tmelt = *_meltingTemp;
    double CTG = *_coldContentFactor;
    double Kf = *_degreeDayFactor;
    double Cn = 0.9 * (*_meanAnnualSnow);
    double SWE = _container->GetContentWithChanges();

    // Update thermal state: exponential smoothing of (T - Tmelt), clamped ≤ 0.
    // _coldContent == 0 means the snow pack is fully "thawed" and melt can occur.
    _coldContent = std::min(0.0, CTG * _coldContent + (1.0 - CTG) * (T - Tmelt));

    if (_coldContent < 0.0 || T <= Tmelt) {
        return {0};
    }

    // Potential melt (cold content fully dissipated)
    double potMelt = Kf * (T - Tmelt);

    // Scale by snow-covered area fraction
    double G_ratio = (Cn > 0) ? std::min(1.0, SWE / Cn) : 1.0;
    // The fraction is corrected between 0.1 and 1 to avoid zero melt when the snowpack is very thin (based on RRMPG)
    double melt = (0.9 * G_ratio + 0.1) * potMelt;

    return {melt};
}
