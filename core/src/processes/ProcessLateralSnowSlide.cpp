#include "ProcessLateralSnowSlide.h"

#include "Brick.h"
#include "HydroUnit.h"
#include "WaterContainer.h"

ProcessLateralSnowSlide::ProcessLateralSnowSlide(WaterContainer* container)
    : ProcessLateral(container),
      _slope_deg(0),
      _coeff(nullptr),
      _exp(nullptr),
      _minSlope(nullptr),
      _maxSlope(nullptr),
      _minSnowHoldingDepth(nullptr) {}

bool ProcessLateralSnowSlide::IsOk() {
    if (_outputs.empty()) {
        wxLogError(_("A SnowSlide process must have at least 1 output."));
        return false;
    }

    return true;
}

void ProcessLateralSnowSlide::RegisterProcessParametersAndForcing(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("coeff", 3178.4f);
    modelSettings->AddProcessParameter("exp", -1.998f);
    modelSettings->AddProcessParameter("min_slope", 10.0f);
    modelSettings->AddProcessParameter("max_slope", 75.0f);
    modelSettings->AddProcessParameter("min_snow_holding_depth", 50.0f);
}

void ProcessLateralSnowSlide::SetHydroUnitProperties(HydroUnit* unit, Brick* brick) {
    _slope_deg = unit->GetPropertyDouble("slope", "degrees");
}

void ProcessLateralSnowSlide::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _coeff = GetParameterValuePointer(processSettings, "coeff");
    _exp = GetParameterValuePointer(processSettings, "exp");
    _minSlope = GetParameterValuePointer(processSettings, "min_slope");
    _maxSlope = GetParameterValuePointer(processSettings, "max_slope");
    _minSnowHoldingDepth = GetParameterValuePointer(processSettings, "min_snow_holding_depth");
}

vecDouble ProcessLateralSnowSlide::GetRates() {
    // Snow density conversion factor
    const float sweToDepthFactor = constants::waterDensity / constants::snowDensity;

    // Current SWE value
    float swe = _container->GetContentWithChanges();  // [mm] Snow water equivalent
    float snowDepth = swe * sweToDepthFactor;  // [mm] Snow depth

    // Snow holding threshold
    float slope = std::max(_slope_deg, *_minSlope);  // [degrees]
    float snowHoldingThresholdMeters = *_coeff * pow(slope, *_exp);  // [m]
    float snowHoldingThreshold = snowHoldingThresholdMeters * 1000.0;  // [mm]

    // Set minimum snow holding depth if slope exceeds maximum slope
    if (_slope_deg > *_maxSlope) {
        snowHoldingThreshold = *_minSnowHoldingDepth;
    }

    // Ensure snow holding threshold is not less than the minimum defined
    snowHoldingThreshold = std::max(snowHoldingThreshold, *_minSnowHoldingDepth);

    // Calculate excess snow to be redistributed
    double excessSwe = 0.0;
    if (snowDepth > snowHoldingThreshold) {
        double excessSnowDepth = snowDepth - snowHoldingThreshold;  // [mm] Excess snow depth
        excessSwe = excessSnowDepth / sweToDepthFactor;  // Convert to SWE [mm]
    }

    // Iterate through each output and calculate the lateral rate
    vecDouble rates(_outputs.size(), 0.0);

    if (excessSwe <= 0.0) {
        // No excess snow to redistribute
        return rates;
    }

    for (size_t i = 0; i < _outputs.size(); ++i) {
        float weight = 1.0;  //TODO: Replace with actual weight calculation
        rates[i] = excessSwe * weight;  // [mm] Lateral snow redistribution rate
    }

    return rates;
}