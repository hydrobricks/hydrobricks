#include "ProcessTransformSnowToIceSwat.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessTransformSnowToIceSwat::ProcessTransformSnowToIceSwat(WaterContainer* container)
    : ProcessTransform(container),
      _basalAccCoeff(nullptr),
      _northHemisphere(nullptr) {}

void ProcessTransformSnowToIceSwat::RegisterProcessParametersAndForcing(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("snow_ice_transformation_basal_acc_coeff", 0.0027f);
    modelSettings->AddProcessParameter("north_hemisphere", 1);
}

void ProcessTransformSnowToIceSwat::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    if (HasParameter(processSettings, "snow_ice_transformation_basal_acc_coeff")) {
        _basalAccCoeff = GetParameterValuePointer(processSettings, "snow_ice_transformation_basal_acc_coeff");
    } else {
        _basalAccCoeff = GetParameterValuePointer(processSettings, "basal_acc_coeff");
    }

    if (HasParameter(processSettings, "north_hemisphere")) {
        _northHemisphere = GetParameterValuePointer(processSettings, "north_hemisphere");
    }
}

vecDouble ProcessTransformSnowToIceSwat::GetRates() {
    bool northHemisphere = !(*_northHemisphere == 0);
    int daysRef = (northHemisphere) ? 81 : 264;  // 81 = March 22, 264 = September 21
    float coeff = *_basalAccCoeff * (1 + std::sin(2.0f * constants::pi * (_currentDayOfYear - daysRef) / 365.0f));

    return {coeff * _container->GetContentWithChanges()};  // [mm/day]
}
