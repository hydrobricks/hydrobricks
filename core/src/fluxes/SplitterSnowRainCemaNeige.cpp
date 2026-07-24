#include "SplitterSnowRainCemaNeige.h"

#include <cmath>

SplitterSnowRainCemaNeige::SplitterSnowRainCemaNeige()
    : _precipitation(nullptr),
      _temperature(nullptr),
      _temperatureMin(nullptr),
      _temperatureMax(nullptr),
      _elevation(std::numeric_limits<double>::quiet_NaN()),
      _rainCorrectionFactor(nullptr),
      _snowCorrectionFactor(nullptr) {}

void SplitterSnowRainCemaNeige::SetParameters(const SplitterSettings& splitterSettings) {
    // No calibrated transition parameters — temperature bounds come from forcings or
    // are hardcoded. Only the optional precipitation correction factors are read.
    _rainCorrectionFactor = GetParameterValuePointerOrUnit(splitterSettings, "rain_correction_factor");
    _snowCorrectionFactor = GetParameterValuePointerOrUnit(splitterSettings, "snow_correction_factor");
}

void SplitterSnowRainCemaNeige::AttachForcing(Forcing* forcing) {
    switch (forcing->GetType()) {
        case VariableType::Precipitation:
            _precipitation = forcing;
            break;
        case VariableType::Temperature:
            _temperature = forcing;
            break;
        case VariableType::TemperatureMin:
            _temperatureMin = forcing;
            break;
        case VariableType::TemperatureMax:
            _temperatureMax = forcing;
            break;
        default:
            throw ModelConfigError("SplitterSnowRainCemaNeige: unsupported forcing type.");
    }
}

double* SplitterSnowRainCemaNeige::GetValuePointer(const string& name) {
    if (name == "rain") {
        return _outputs[0]->GetAmountPointer();
    }
    if (name == "snow") {
        return _outputs[1]->GetAmountPointer();
    }
    throw ModelConfigError(std::format("SplitterSnowRainCemaNeige: unknown output '{}'.", name));
}

void SplitterSnowRainCemaNeige::SetHydroUnitProperties(HydroUnit* unit) {
    _elevation = unit->GetPropertyDouble("elevation", "m");
}

bool SplitterSnowRainCemaNeige::IsValid() const {
    if (_outputs.size() != 2) {
        LogError("SplitterSnowRainCemaNeige: exactly 2 outputs required (rain, snow).");
        return false;
    }
    if (_precipitation == nullptr) {
        LogError("SplitterSnowRainCemaNeige: precipitation forcing not attached.");
        return false;
    }
    if (_temperature == nullptr) {
        LogError("SplitterSnowRainCemaNeige: temperature forcing not attached.");
        return false;
    }
    if (_elevation < 1500.0 && (_temperatureMin == nullptr || _temperatureMax == nullptr)) {
        LogError(
            "SplitterSnowRainCemaNeige: temperature_min and temperature_max forcings are required for "
            "HydroUnits below 1500 m elevation.");
        return false;
    }
    if (std::isnan(_elevation)) {
        LogError("SplitterSnowRainCemaNeige: HydroUnit elevation was not set.");
        return false;
    }
    return true;
}

void SplitterSnowRainCemaNeige::Compute() {
    double fsolid = 0.0;

    if (_elevation >= 1500.0) {
        double t = _temperature->GetValue();

        if (t <= -1.0) {
            fsolid = 1.0;
        } else if (t >= 3.0) {
            fsolid = 0.0;
        } else {
            fsolid = (3.0 - t) / 4.0;  // linear transition between -1 and 3 °C
        }
    } else {
        double tmin = _temperatureMin->GetValue();
        double tmax = _temperatureMax->GetValue();

        if (tmax < tmin) {
            LogError(
                std::format("SplitterSnowRainCemaNeige: temperature_max ({:.1f} °C) "
                            "is less than temperature_min ({:.1f} °C).",
                            tmax, tmin));
            tmax = tmin;  // avoid NaN in fsolid calculation below
        }

        if (tmax <= 0) {
            fsolid = 1.0;
        } else if (tmin >= 0) {
            fsolid = 0.0;
        } else {
            fsolid = 1 - (tmax / (tmax - tmin));  // linear transition between tmin and tmax
        }
    }

    double P = _precipitation->GetValue();
    _outputs[0]->UpdateFlux(P * (1.0 - fsolid) * *_rainCorrectionFactor);  // rain
    _outputs[1]->UpdateFlux(P * fsolid * *_snowCorrectionFactor);          // snow
}
