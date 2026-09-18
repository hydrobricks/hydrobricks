#include "ProcessETWetSurfacePrevah.h"

#include <algorithm>

#include "HydroUnit.h"
#include "LandCover.h"
#include "ProcessOutflowSplit.h"
#include "TimeMachine.h"
#include "WaterContainer.h"

ProcessETWetSurfacePrevah::ProcessETWetSurfacePrevah(WaterContainer* container)
    : ProcessETOpenWaterPrevah(container) {}

bool ProcessETWetSurfacePrevah::IsValid() const {
    if (!ProcessETOpenWaterPrevah::IsValid()) {
        return false;
    }
    if (_wetlands.empty()) {
        LogError("PREVAH wet-surface ET process: missing the gate brick (wetland land cover).");
        return false;
    }

    return true;
}

void ProcessETWetSurfacePrevah::AddGateBrick(Brick* brick) {
    auto* landCover = dynamic_cast<LandCover*>(brick);
    if (landCover == nullptr) {
        throw ModelConfigError("PREVAH wet-surface ET process: the gate brick must be a land cover.");
    }

    ProcessOutflowSplit* split = nullptr;
    for (int i = 0; i < brick->GetProcessCount(); ++i) {
        split = dynamic_cast<ProcessOutflowSplit*>(brick->GetProcess(i));
        if (split != nullptr) {
            break;
        }
    }
    if (split == nullptr) {
        throw ModelConfigError(
            std::format("PREVAH wet-surface ET process: the gate brick '{}' has no 'outflow:split' process "
                        "(wet fraction).",
                        brick->GetName()));
    }

    _wetlands.push_back(landCover);
    _wetSplits.push_back(split);
}

double ProcessETWetSurfacePrevah::GetWetShare() const {
    double wetShare = 0.0;
    for (size_t i = 0; i < _wetlands.size(); ++i) {
        wetShare += _wetlands[i]->GetAreaFraction() * _wetSplits[i]->GetSplitFraction();
    }

    return std::clamp(wetShare, 0.0, 1.0);
}

const vecDouble& ProcessETWetSurfacePrevah::GetRates() {
    double wetShare = GetWetShare();
    if (wetShare <= 0.0) {
        return StoreRates({0});
    }

    double albedo = _hydroUnit->GetSnowAlbedo(*_albedoLand);
    double rate = *_etFactor * wetShare * (1.0 - albedo) / 0.8 * GetForcingRate(_pet);

    if (_timeMachine != nullptr) {
        double maxRate = _container->GetContentWithChanges() / GetTimeStepInDays();
        rate = std::min(rate, std::max(0.0, maxRate));
    }

    return StoreRates({rate});
}
