#include "ProcessPercolationPrevah.h"

#include <algorithm>

#include "Brick.h"
#include "WaterContainer.h"

ProcessPercolationPrevah::ProcessPercolationPrevah(WaterContainer* container)
    : ProcessOutflow(container),
      _rate(nullptr),
      _thresholdFraction(nullptr) {}

void ProcessPercolationPrevah::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("percolation_rate", 0.1f);
    modelSettings->AddProcessParameter("threshold_fraction", 0.7f);
}

bool ProcessPercolationPrevah::IsValid() const {
    if (!ProcessOutflow::IsValid()) {
        return false;
    }
    if (_gateBricks.empty()) {
        LogError("PREVAH percolation process: missing the gate brick (soil moisture store).");
        return false;
    }
    for (auto* gateBrick : _gateBricks) {
        if (gateBrick == nullptr) {
            LogError("PREVAH percolation process: an unresolved gate brick.");
            return false;
        }
    }
    if (_rate == nullptr) {
        LogError("PREVAH percolation process: missing the 'percolation_rate' parameter.");
        return false;
    }
    if (_thresholdFraction == nullptr) {
        LogError("PREVAH percolation process: missing the 'threshold_fraction' parameter.");
        return false;
    }
    if (*_thresholdFraction < 0 || *_thresholdFraction >= 1) {
        LogError("PREVAH percolation process: the 'threshold_fraction' parameter must be in [0, 1).");
        return false;
    }

    return true;
}

void ProcessPercolationPrevah::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _rate = GetParameterValuePointer(processSettings, "percolation_rate");
    _thresholdFraction = GetParameterValuePointer(processSettings, "threshold_fraction");
}

const vecDouble& ProcessPercolationPrevah::GetRates() {
    // Sum over the gate bricks: with one soil moisture store per land cover, the
    // contents already carry the cover area fractions, so the ratio of the sums is
    // the area-weighted mean saturation of the hydro unit.
    double content = 0;
    double capacity = 0;
    for (auto* gateBrick : _gateBricks) {
        WaterContainer* container = gateBrick->GetWaterContainer();
        capacity += container->GetMaximumCapacity();
        content += container->GetContentWithChanges();
    }

    if (capacity <= 0) {
        return StoreRates({0});
    }

    double fillingRatio = content / capacity;
    double threshold = static_cast<double>(*_thresholdFraction);
    double gating = std::clamp((fillingRatio - threshold) / (1.0 - threshold), 0.0, 1.0);

    return StoreRates({(*_rate) * gating});
}
