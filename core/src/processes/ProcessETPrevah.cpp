#include "ProcessETPrevah.h"

#include <algorithm>

#include "HydroUnit.h"
#include "WaterContainer.h"

ProcessETPrevah::ProcessETPrevah(WaterContainer* container)
    : ProcessETHBV(container),
      _albedoLand(nullptr) {}

void ProcessETPrevah::RegisterProcessSettings(SettingsModel* modelSettings) {
    ProcessETHBV::RegisterProcessSettings(modelSettings);
    modelSettings->AddProcessParameter("albedo_land", 0.2f);
}

bool ProcessETPrevah::IsValid() const {
    if (!ProcessETHBV::IsValid()) {
        return false;
    }
    if (_hydroUnit == nullptr) {
        LogError("PREVAH ET process requires a hydro-unit context (hydro-unit bricks only).");
        return false;
    }
    if (_albedoLand == nullptr) {
        LogError("PREVAH ET process: missing the 'albedo_land' parameter.");
        return false;
    }
    if (*_albedoLand < 0 || *_albedoLand >= 1) {
        LogError("PREVAH ET process: the 'albedo_land' parameter must be in [0, 1).");
        return false;
    }

    return true;
}

void ProcessETPrevah::SetParameters(const ProcessSettings& processSettings) {
    ProcessETHBV::SetParameters(processSettings);
    _albedoLand = GetParameterValuePointer(processSettings, "albedo_land");
}

const vecDouble& ProcessETPrevah::GetRates() {
    assert(_container->HasMaximumCapacity());

    // PREVAH snow-albedo reduction of the potential rate: (1 - albedo)/0.8, with the
    // area-weighted albedo combining the snow-free ground and the (age-dependent) snow
    // albedo of the unit's snowpacks. Neutral (factor 1) when snow-free at
    // albedo_land = 0.2.
    double albedo = _hydroUnit->GetSnowAlbedo(*_albedoLand);
    double albedoFactor = (1.0 - albedo) / 0.8;

    double pet = static_cast<double>(*_etCorrectionFactor) * albedoFactor * _pet->GetValue();
    double threshold = static_cast<double>(*_lp) * _container->GetMaximumCapacity();
    if (threshold <= 0) {
        return StoreRates({pet});
    }

    double ratio = std::min(1.0, _container->GetContentWithChanges() / threshold);

    return StoreRates({pet * ratio});
}
