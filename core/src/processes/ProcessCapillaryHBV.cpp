#include "ProcessCapillaryHBV.h"

#include <algorithm>

#include "Brick.h"
#include "LandCover.h"
#include "WaterContainer.h"

ProcessCapillaryHBV::ProcessCapillaryHBV(WaterContainer* container)
    : ProcessOutflow(container),
      _maxCapillaryFlux(nullptr) {}

void ProcessCapillaryHBV::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("max_capillary_flux", 0.0f);
}

void ProcessCapillaryHBV::AddTargetBrickWithWeights(Brick* targetBrick, const std::vector<Brick*>& weightSources) {
    _targetBricks.push_back(targetBrick);

    std::vector<const double*> fractions;
    for (Brick* source : weightSources) {
        auto landCover = dynamic_cast<LandCover*>(source);
        if (landCover != nullptr) {
            fractions.push_back(landCover->GetAreaFractionPointer());
        }
    }
    _weights.push_back(fractions);
}

bool ProcessCapillaryHBV::IsValid() const {
    if (_targetBricks.empty()) {
        LogError("HBV capillary process must be linked to at least one target brick (soil moisture).");
        return false;
    }
    if (static_cast<int>(_targetBricks.size()) != GetConnectionCount()) {
        LogError("HBV capillary process: the number of target bricks does not match the number of outputs.");
        return false;
    }
    if (_maxCapillaryFlux == nullptr) {
        LogError("HBV capillary process: missing the 'max_capillary_flux' parameter.");
        return false;
    }

    // The area weighting is derived from the land covers feeding each target soil. A
    // target with no feeding land cover is treated as the full unit area (w = 1),
    // which is meaningful for a single target but over-counts when several targets
    // each take the full area. That combination is not used by HBV (soils are always
    // cover-fed), so it is flagged as a likely misconfiguration rather than allowed
    // silently.
    if (_targetBricks.size() > 1) {
        for (const auto& weights : _weights) {
            if (weights.empty()) {
                LogError(
                    "HBV capillary process fans out to several targets but one is not fed by any land cover, "
                    "so its area weight is undefined.");
                return false;
            }
        }
    }

    return true;
}

void ProcessCapillaryHBV::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _maxCapillaryFlux = GetParameterValuePointer(processSettings, "max_capillary_flux");
}

const vecDouble& ProcessCapillaryHBV::GetRates() {
    _changeRates.assign(_targetBricks.size(), 0.0);

    for (size_t i = 0; i < _targetBricks.size(); ++i) {
        WaterContainer* target = _targetBricks[i]->GetWaterContainer();
        if (target->GetMaximumCapacity() <= 0) {
            continue;
        }

        // Area weight: the unit fraction feeding this soil (sum of the feeding land
        // covers' live area fractions). A single soil-bearing cover gives w = 1, i.e.
        // the original single-target behaviour. A target with no feeding land cover
        // falls back to the full unit area (w = 1); IsValid() rejects this fallback
        // when there are several targets (the weighting would then be undefined).
        double weight = 0.0;
        for (const double* fraction : _weights[i]) {
            weight += *fraction;
        }
        if (_weights[i].empty()) {
            weight = 1.0;
        }

        double deficit = 1.0 - std::clamp(target->GetTargetFillingRatio(), 0.0, 1.0);
        _changeRates[i] = (*_maxCapillaryFlux) * weight * deficit;
    }

    return _changeRates;
}
