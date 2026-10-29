#include "ProcessETPrevah.h"

#include <algorithm>

#include "HydroUnit.h"
#include "LandCover.h"
#include "WaterContainer.h"

ProcessETPrevah::ProcessETPrevah(WaterContainer* container)
    : ProcessETHBV(container),
      _albedoLand(nullptr),
      _potentialFraction(nullptr),
      _cover(nullptr) {}

void ProcessETPrevah::RegisterProcessSettings(SettingsModel* modelSettings) {
    ProcessETHBV::RegisterProcessSettings(modelSettings);
    modelSettings->AddProcessParameter("albedo_land", 0.2f);
    modelSettings->AddProcessParameter("potential_fraction", 0.0f);
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
    if (_potentialFraction == nullptr) {
        LogError("PREVAH ET process: missing the 'potential_fraction' parameter.");
        return false;
    }
    if (*_potentialFraction < 0 || *_potentialFraction > 1) {
        LogError("PREVAH ET process: the 'potential_fraction' parameter must be in [0, 1].");
        return false;
    }

    return true;
}

void ProcessETPrevah::SetParameters(const ProcessSettings& processSettings) {
    ProcessETHBV::SetParameters(processSettings);
    _albedoLand = GetParameterValuePointer(processSettings, "albedo_land");
    _potentialFraction = GetParameterValuePointer(processSettings, "potential_fraction");
}

void ProcessETPrevah::AddGateBrick(Brick* brick) {
    auto* landCover = dynamic_cast<LandCover*>(brick);
    if (landCover == nullptr) {
        throw ModelConfigError("PREVAH ET process: the gate brick must be a land cover.");
    }
    _cover = landCover;
}

const vecDouble& ProcessETPrevah::GetRates() {
    assert(_container->HasMaximumCapacity());

    // PREVAH snow-albedo reduction of the potential rate: (1 - albedo)/0.8, with the
    // area-weighted albedo combining the snow-free ground and the (age-dependent) snow
    // albedo of the unit's snowpacks. Neutral (factor 1) when snow-free at
    // albedo_land = 0.2. With a cover given (a soil store of its own), only the snow of
    // that cover is seen.
    double albedo = _hydroUnit->GetSnowAlbedo(*_albedoLand, _cover);
    double albedoFactor = (1.0 - albedo) / 0.8;

    double pet = static_cast<double>(*_etCorrectionFactor) * albedoFactor * GetPetRate(_pet);

    // The soil-moisture branch of s_eta_soil. A threshold of zero is not the limit of
    // the formula here but PREVAH's 'IF (sgrlp .le. 0.0) THEN EA = 0.0': a cover with
    // no plant-available water evaporates nothing.
    double threshold = static_cast<double>(*_lp) * _container->GetMaximumCapacity();
    double limited = 0;
    if (threshold > 0) {
        limited = pet * std::min(1.0, _container->GetContentWithChanges() / threshold);
    }

    double potentialShare = static_cast<double>(*_potentialFraction);

    return StoreRates({potentialShare * pet + (1.0 - potentialShare) * limited});
}
