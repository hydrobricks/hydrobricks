#include "ProcessLateralSnowSlide.h"

#include "Brick.h"
#include "FluxToBrick.h"
#include "HydroUnit.h"
#include "Snowpack.h"
#include "WaterContainer.h"

ProcessLateralSnowSlide::ProcessLateralSnowSlide(WaterContainer* container)
    : ProcessLateral(container),
      _slope_deg(0),
      _coeff(nullptr),
      _exp(nullptr),
      _minSlope(nullptr),
      _maxSlope(nullptr),
      _minSnowHoldingDepth(nullptr),
      _maxSnowDepth(nullptr) {}

bool ProcessLateralSnowSlide::IsOk() {
    return true;
}

void ProcessLateralSnowSlide::RegisterProcessParametersAndForcing(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("coeff", 3178.4f);
    modelSettings->AddProcessParameter("exp", -1.998f);
    modelSettings->AddProcessParameter("min_slope", 10.0f);
    modelSettings->AddProcessParameter("max_slope", 75.0f);
    modelSettings->AddProcessParameter("min_snow_holding_depth", 50.0f);
    modelSettings->AddProcessParameter("max_snow_depth", 20000.0f);  // 20 m of snow depth = 5 m of SWE
}

void ProcessLateralSnowSlide::SetHydroUnitProperties(HydroUnit* unit, Brick*) {
    _slope_deg = static_cast<float>(unit->GetPropertyDouble("slope", "degrees"));
}

void ProcessLateralSnowSlide::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _coeff = GetParameterValuePointer(processSettings, "coeff");
    _exp = GetParameterValuePointer(processSettings, "exp");
    _minSlope = GetParameterValuePointer(processSettings, "min_slope");
    _maxSlope = GetParameterValuePointer(processSettings, "max_slope");
    _minSnowHoldingDepth = GetParameterValuePointer(processSettings, "min_snow_holding_depth");
    _maxSnowDepth = GetParameterValuePointer(processSettings, "max_snow_depth");
}

vecDouble ProcessLateralSnowSlide::GetRates() {
    // If no fluxes attached (no connection), return empty vector
    if (_outputs.empty()) {
        return {};
    }

    // Snow density conversion factor
    const float sweToDepthFactor = constants::waterDensity / constants::snowDensity;

    // Current SWE value
    double swe = _container->GetContentWithChanges();  // [mm] Snow Water Equivalent
    double snowDepth = swe * sweToDepthFactor;         // [mm] Snow depth calculated from SWE

    // Snow holding threshold
    float slope = std::max(_slope_deg, *_minSlope);                     // [degrees]
    double snowHoldingThresholdMeters = *_coeff * pow(slope, *_exp);    // [m]
    double snowHoldingThreshold = snowHoldingThresholdMeters * 1000.0;  // [mm]

    // Set minimum snow holding depth if slope exceeds maximum slope
    if (_slope_deg > *_maxSlope) {
        snowHoldingThreshold = *_minSnowHoldingDepth;
    }

    // Ensure snow holding threshold is not less than the minimum defined
    snowHoldingThreshold = std::max<double>(snowHoldingThreshold, *_minSnowHoldingDepth);

    // If a maximum snow depth is defined, ensure it does not exceed it
    if (*_maxSnowDepth > 0.0f) {
        snowHoldingThreshold = std::min<double>(snowHoldingThreshold, *_maxSnowDepth);
    }

    // Calculate excess snow to be redistributed
    double excessSwe = 0.0;
    if (snowDepth > snowHoldingThreshold) {
        double excessSnowDepth = snowDepth - snowHoldingThreshold;  // [mm] Excess snow depth beyond threshold
        excessSwe = excessSnowDepth / sweToDepthFactor;             // [mm] Convert excess depth to SWE
    }

    // Iterate through each output and calculate the lateral rate
    vecDouble rates(_outputs.size(), 0.0);

    if (excessSwe <= 0.0) {
        // No excess snow to redistribute
        return rates;
    }

    // Cap the excess SWE to a maximum of 1000 mm to prevent unrealistic redistribution rates
    if (excessSwe > 1000.0) {
        wxLogDebug(_("Snow redistribution: excess SWE (%f mm) is too high, capping to 1000 mm."), excessSwe);
        excessSwe = 1000.0;
    }

    for (size_t i = 0; i < _outputs.size(); ++i) {
        // The weight of the process rate is adjusted so that when subtracted, the correct amount of SWE leaves.
        wxASSERT(_weights.size() > i);
        double targetFraction = GetTargetLandCoverAreaFraction(_outputs[i]);
        if (targetFraction <= PRECISION) {
            rates[i] = 0.0;  // No redistribution if target fraction is negligible
            continue;
        }
        rates[i] = excessSwe * _weights[i] * targetFraction;  // [mm] Redistribution rate to target unit

        rates[i] = AvoidUnrealisticAccumulation(rates[i], _outputs[i]);

        // The weight of the flux is adjusted to account for the area ratio between the source and target land cover.
        // As it can change (e.g., due to land cover changes), we compute it dynamically.
        double fractionAreas = ComputeFractionAreas(_outputs[i]);
        _outputs[i]->SetFractionUnitArea(fractionAreas);  // Adjust flux weight by area ratio
    }

    return rates;
}

double ProcessLateralSnowSlide::AvoidUnrealisticAccumulation(double rate, Flux* flux) {
    // Do not redistribute snow if the target snowpack has more than 1.5 times the overall maximum snow depth.
    // This avoids unrealistic accumulation in the target snowpack.
    auto fluxToBrick = dynamic_cast<FluxToBrick*>(flux);
    wxASSERT(fluxToBrick);
    Brick* targetBrick = fluxToBrick->GetTargetBrick();
    wxASSERT(targetBrick);
    auto targetSnowpack = dynamic_cast<Snowpack*>(targetBrick);
    double targetSwe = targetSnowpack->GetContent(ContentType::Snow);
    const float sweToDepthFactor = constants::waterDensity / constants::snowDensity;
    if (*_maxSnowDepth > 0.0f && targetSwe > 1.5 * (*_maxSnowDepth) / sweToDepthFactor) {
        return 0;
    }
    return rate;
}