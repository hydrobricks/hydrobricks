#include "ProcessLateralSnowRedistributionFrey.h"

#include <algorithm>
#include <cmath>

#include "Brick.h"
#include "FluxToBrick.h"
#include "Forcing.h"
#include "GlobVars.h"
#include "HydroUnit.h"
#include "Snowpack.h"
#include "TimeMachine.h"
#include "WaterContainer.h"

ProcessLateralSnowRedistributionFrey::ProcessLateralSnowRedistributionFrey(WaterContainer* container,
                                                                           bool dynamicDensity)
    : ProcessLateral(container),
      _dynamicDensity(dynamicDensity),
      _slopeDeg(0),
      _temperature(nullptr),
      _correction(nullptr),
      _holdingCapacity(nullptr),
      _rhoMax(nullptr),
      _maxSnowDepth(nullptr),
      _snowDensity(nullptr),
      _rhoMin(nullptr),
      _rhoFreshMax(nullptr),
      _rhoSettling(nullptr),
      _rhoScale(nullptr),
      _tScale(nullptr),
      _density(constants::snowDensity),
      _prevSwe(0.0) {}

bool ProcessLateralSnowRedistributionFrey::IsValid() const {
    // Permissive (like snow_slide): hydro units without lateral connections simply do not redistribute.
    if (_dynamicDensity && _temperature == nullptr) {
        LogError("The dynamic snow redistribution (Frey & Holzmann) requires temperature forcing.");
        return false;
    }
    return true;
}

void ProcessLateralSnowRedistributionFrey::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("correction", 1.0f);                      // Correction coefficient C
    modelSettings->AddProcessParameter("snow_holding_capacity", 200.0f);         // H_v (snow depth) [mm]
    modelSettings->AddProcessParameter("rho_max", 450.0f);                       // Max snow density for redistribution
    modelSettings->AddProcessParameter("snow_density", constants::snowDensity);  // Constant snow density
    modelSettings->AddProcessParameter("max_snow_depth", 20000.0f);              // 20 m of snow depth = 5 m of SWE
}

void ProcessLateralSnowRedistributionFrey::RegisterProcessSettingsDynamic(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("correction", 1.0f);               // Correction coefficient C
    modelSettings->AddProcessParameter("snow_holding_capacity", 200.0f);  // H_v (snow depth) [mm]
    modelSettings->AddProcessParameter("rho_max", 450.0f);                // Max snow density (settling target)
    modelSettings->AddProcessParameter("rho_min", 100.0f);                // Min snow density
    modelSettings->AddProcessParameter("rho_fresh_max", 300.0f);          // Max fresh-snow density
    modelSettings->AddProcessParameter("rho_settling", 0.1f);             // Settling rate toward rho_max [1/day]
    modelSettings->AddProcessParameter("rho_scale", 1.2f);                // Density sigmoid slope coefficient
    modelSettings->AddProcessParameter("t_scale", 1.0f);                  // Density sigmoid shift coefficient
    modelSettings->AddProcessParameter("max_snow_depth", 20000.0f);       // 20 m of snow depth = 5 m of SWE
    modelSettings->AddProcessForcing("temperature");
}

void ProcessLateralSnowRedistributionFrey::SetHydroUnitProperties(HydroUnit* unit, Brick*) {
    _slopeDeg = static_cast<float>(unit->GetPropertyDouble("slope", "degrees"));
}

void ProcessLateralSnowRedistributionFrey::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _correction = GetParameterValuePointer(processSettings, "correction");
    _holdingCapacity = GetParameterValuePointer(processSettings, "snow_holding_capacity");
    _rhoMax = GetParameterValuePointer(processSettings, "rho_max");
    _maxSnowDepth = GetParameterValuePointer(processSettings, "max_snow_depth");
    if (_dynamicDensity) {
        _rhoMin = GetParameterValuePointer(processSettings, "rho_min");
        _rhoFreshMax = GetParameterValuePointer(processSettings, "rho_fresh_max");
        _rhoSettling = GetParameterValuePointer(processSettings, "rho_settling");
        _rhoScale = GetParameterValuePointer(processSettings, "rho_scale");
        _tScale = GetParameterValuePointer(processSettings, "t_scale");
    } else {
        _snowDensity = GetParameterValuePointer(processSettings, "snow_density");
    }
}

void ProcessLateralSnowRedistributionFrey::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == VariableType::Temperature) {
        _temperature = forcing;
    } else {
        throw ModelConfigError("Forcing must be of type Temperature");
    }
}

void ProcessLateralSnowRedistributionFrey::Reset() {
    ProcessLateral::Reset();
    _density = constants::snowDensity;
    _prevSwe = 0.0;
}

double ProcessLateralSnowRedistributionFrey::ComputeFreshSnowDensity() const {
    // Sigmoid of the transition temperature (paper Eqs. 8-9).
    double rhoMin = static_cast<double>(*_rhoMin);
    double rhoFreshMax = static_cast<double>(*_rhoFreshMax);
    double tAir = (_temperature != nullptr) ? _temperature->GetValue() : 0.0;
    double tTr = tAir / static_cast<double>(*_rhoScale) + static_cast<double>(*_tScale);
    double sig = tTr / std::sqrt(1.0 + tTr * tTr);  // in [-1, 1]
    return (rhoFreshMax - rhoMin) * (sig + 1.0) * 0.5 + rhoMin;
}

void ProcessLateralSnowRedistributionFrey::Finalize() {
    if (!_dynamicDensity) {
        return;
    }

    // Advance the tracked snow density exactly once per time step, using the committed SWE.
    double swe = _container->GetContentWithoutChanges();  // [mm]
    double dt = (_timeMachine != nullptr) ? *_timeMachine->GetTimeStepPointer() : 1.0;

    // Settle the existing snow toward the maximum density (simplified Eq. 10).
    double rhoMax = static_cast<double>(*_rhoMax);
    double settleFactor = std::clamp(static_cast<double>(*_rhoSettling) * dt, 0.0, 1.0);
    double settledDensity = _density + (rhoMax - _density) * settleFactor;

    // Fresh snow is inferred from the step-to-step SWE increase, which cannot distinguish new snowfall
    // from snow received via redistribution; acceptor inflow is therefore also counted as "fresh" and
    // mixed in at the temperature-derived fresh-snow density. This is a simplification of the density
    // bookkeeping (hydrobricks tracks only SWE, not the donor density of transported snow).
    double fresh = std::max(swe - _prevSwe, 0.0);
    double existing = std::max(std::min(_prevSwe, swe), 0.0);

    if (swe <= 0.0) {
        _density = ComputeFreshSnowDensity();
    } else if (fresh > 0.0 && (existing + fresh) > 0.0) {
        double rhoFresh = ComputeFreshSnowDensity();
        _density = (existing * settledDensity + fresh * rhoFresh) / (existing + fresh);
    } else {
        _density = settledDensity;
    }

    _prevSwe = swe;
}

double ProcessLateralSnowRedistributionFrey::GetSnowDensity() const {
    if (!_dynamicDensity) {
        return static_cast<double>(*_snowDensity);
    }
    return (_density > 0.0) ? _density : constants::snowDensity;
}

const vecDouble& ProcessLateralSnowRedistributionFrey::GetRates() {
    // If no fluxes attached (no connection), return empty vector.
    if (_outputs.empty()) {
        return StoreRates({});
    }

    _changeRates.assign(_outputs.size(), 0.0);

    double swe = _container->GetContentWithChanges();  // [mm] Snow Water Equivalent
    if (swe <= 0.0) {
        return _changeRates;
    }

    double rho = GetSnowDensity();
    double rhoMax = static_cast<double>(*_rhoMax);

    // Snow holding capacity H_v is provided as a snow-depth threshold [mm]; convert it to SWE using the
    // same density used in f_rho, keeping the two self-consistent.
    double sweToDepthFactor = constants::waterDensity / rho;
    double holdingSwe = static_cast<double>(*_holdingCapacity) / sweToDepthFactor;

    double excessSwe = std::max(swe - holdingSwe, 0.0);
    if (excessSwe <= 0.0) {
        return _changeRates;
    }

    // Distribution coefficient f_rho (paper Eq. 13). Dense snow (rho >= rhoMax) and flat terrain do not
    // redistribute.
    double slope = std::clamp(static_cast<double>(_slopeDeg), 0.0, 90.0);
    double fRho = 0.0;
    if (rho < rhoMax) {
        fRho = ((rhoMax - rho) / rhoMax) * std::exp(-rho / rhoMax) * (slope / 90.0);
    }
    if (fRho <= 0.0) {
        return _changeRates;
    }

    double redistributed = excessSwe * fRho * static_cast<double>(*_correction);  // [mm SWE]

    // Cap to prevent unrealistic redistribution rates (mirrors snow_slide).
    if (redistributed > 1000.0) {
        LogDebug("Snow redistribution: amount ({} mm) is too high, capping to 1000 mm.", redistributed);
        redistributed = 1000.0;
    }

    for (size_t i = 0; i < _outputs.size(); ++i) {
        assert(_weights.size() > i);
        Flux* flux = _outputs[i].get();
        double targetFraction = GetTargetLandCoverAreaFraction(flux);
        if (NearlyZero(targetFraction, PRECISION)) {
            _changeRates[i] = 0.0;  // No redistribution if target fraction is negligible
            continue;
        }
        _changeRates[i] = redistributed * _weights[i] * targetFraction;  // [mm] Redistribution rate to target

        _changeRates[i] = AvoidUnrealisticAccumulation(_changeRates[i], flux);

        // The flux weight is adjusted to account for the area ratio between the source and target land cover.
        // As it can change (e.g., due to land cover changes), we compute it dynamically.
        double fractionAreas = ComputeFractionAreas(flux);
        flux->SetFractionUnitArea(fractionAreas);
    }

    return _changeRates;
}

double ProcessLateralSnowRedistributionFrey::AvoidUnrealisticAccumulation(double rate, Flux* flux) {
    // Do not redistribute snow if the target snowpack has more than 1.5 times the overall maximum snow
    // depth. This avoids unrealistic accumulation in the target snowpack.
    auto fluxToBrick = dynamic_cast<FluxToBrick*>(flux);
    assert(fluxToBrick);
    Brick* targetBrick = fluxToBrick->GetTargetBrick();
    assert(targetBrick);
    auto targetSnowpack = dynamic_cast<Snowpack*>(targetBrick);
    double targetSwe = targetSnowpack->GetContent(ContentType::Snow);
    double sweToDepthFactor = constants::waterDensity / GetSnowDensity();
    if (*_maxSnowDepth > 0.0f && targetSwe > 1.5 * static_cast<double>(*_maxSnowDepth) / sweToDepthFactor) {
        return 0;
    }
    return rate;
}
