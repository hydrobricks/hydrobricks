#include <cmath>
#include <limits>

#include "SplitterSnowRainLinearCemaNeige.h"

SplitterSnowRainLinearCemaNeige::SplitterSnowRainLinearCemaNeige()
    : SplitterSnowRainLinear(),
      _elevation(std::numeric_limits<double>::quiet_NaN()) {}

bool SplitterSnowRainLinearCemaNeige::IsValid() const {
    if (!SplitterSnowRainLinear::IsValid()) {
        return false;
    }
    if (std::isnan(_elevation)) {
        LogError("SplitterSnowRainLinearCemaNeige: HydroUnit elevation was not set (missing 'elevation' property).");
        return false;
    }
    return true;
}

void SplitterSnowRainLinearCemaNeige::SetHydroUnitProperties(HydroUnit* unit) {
    _elevation = unit->GetPropertyDouble("elevation", "m");
}

void SplitterSnowRainLinearCemaNeige::Compute() {
    double tmin, tmax;
    if (_elevation >= 1500.0) {
        // Hardcoded thresholds for high-elevation zones (Valéry et al., 2014)
        tmin = -1.0;
        tmax = 3.0;
    } else {
        tmin = *_transitionStart;
        tmax = *_transitionEnd;
    }

    double T = _temperature->GetValue();
    double P = _precipitation->GetValue();

    if (T <= tmin) {
        _outputs[0]->UpdateFlux(0);
        _outputs[1]->UpdateFlux(P);
    } else if (T >= tmax) {
        _outputs[0]->UpdateFlux(P);
        _outputs[1]->UpdateFlux(0);
    } else {
        double rainFraction = (T - tmin) / (tmax - tmin);
        _outputs[0]->UpdateFlux(P * rainFraction);
        _outputs[1]->UpdateFlux(P * (1 - rainFraction));
    }
}
