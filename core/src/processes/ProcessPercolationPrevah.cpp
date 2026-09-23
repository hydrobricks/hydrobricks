#include "ProcessPercolationPrevah.h"

#include "Brick.h"
#include "Snowpack.h"
#include "WaterContainer.h"

ProcessPercolationPrevah::ProcessPercolationPrevah(WaterContainer* container)
    : ProcessOutflow(container),
      _rate(nullptr),
      _thresholdFraction(nullptr),
      _conductivityFactor(nullptr),
      _constantFraction(nullptr) {}

void ProcessPercolationPrevah::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("percolation_rate", 0.1f);
    modelSettings->AddProcessParameter("threshold_fraction", 0.7f);
    modelSettings->AddProcessParameter("conductivity_factor", 1.0f);
    modelSettings->AddProcessParameter("constant_fraction", 0.0f);
}

void ProcessPercolationPrevah::AddGateBrick(Brick* brick) {
    // The snowpacks only switch the constant branch off; the other gate bricks are the
    // soil moisture stores.
    if (auto* snowpack = dynamic_cast<Snowpack*>(brick)) {
        _snowpacks.push_back(snowpack);
        return;
    }
    _gateBricks.push_back(brick);
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
    if (_conductivityFactor == nullptr || *_conductivityFactor < 0) {
        LogError("PREVAH percolation process: the 'conductivity_factor' parameter must be positive.");
        return false;
    }
    if (_constantFraction == nullptr || *_constantFraction < 0 || *_constantFraction > 1) {
        LogError("PREVAH percolation process: the 'constant_fraction' parameter must be in [0, 1].");
        return false;
    }

    return true;
}

void ProcessPercolationPrevah::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _rate = GetParameterValuePointer(processSettings, "percolation_rate");
    _thresholdFraction = GetParameterValuePointer(processSettings, "threshold_fraction");
    _conductivityFactor = GetParameterValuePointer(processSettings, "conductivity_factor");
    _constantFraction = GetParameterValuePointer(processSettings, "constant_fraction");
}

double ProcessPercolationPrevah::GetSnowCoveredFraction() const {
    // PREVAH counts a hydrotope as snow covered above 0.1 mm SWE.
    constexpr double snowThreshold = 0.1;
    double area = 0;
    double snowCoveredArea = 0;
    for (auto* snowpack : _snowpacks) {
        double fraction = snowpack->GetParentAreaFraction();
        area += fraction;
        if (snowpack->GetContent(ContentType::Snow) > snowThreshold) {
            snowCoveredArea += fraction;
        }
    }

    return area > 0 ? snowCoveredArea / area : 0;
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

    double rate = *_rate;
    double conductivityFactor = *_conductivityFactor;

    // Soil-moisture-gated rate: the full rate at saturation, the ramp (scaled by the
    // conductivity factor) between the ET limit and the capacity, none below.
    double gated = 0;
    if (capacity > 0) {
        double fillingRatio = content / capacity;
        double threshold = static_cast<double>(*_thresholdFraction);
        if (fillingRatio >= 1.0) {
            gated = rate;
        } else if (fillingRatio > threshold) {
            gated = rate * conductivityFactor * (fillingRatio - threshold) / (1.0 - threshold);
        }
    }

    // The share following the constant branch percolates at the constant rate where it
    // is free of snow, and not at all under snow.
    double constantFraction = *_constantFraction;
    if (constantFraction <= 0) {
        return StoreRates({gated});
    }
    double snowFree = 1.0 - GetSnowCoveredFraction();

    return StoreRates({(1.0 - constantFraction) * gated + constantFraction * snowFree * rate * conductivityFactor});
}
