#include "ProcessETOpenWater.h"

#include <algorithm>

#include "TimeMachine.h"
#include "WaterContainer.h"

ProcessETOpenWater::ProcessETOpenWater(WaterContainer* container)
    : ProcessET(container),
      _pet(nullptr) {}

void ProcessETOpenWater::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessForcing("pet");
}

bool ProcessETOpenWater::IsValid() const {
    if (!ProcessET::IsValid()) {
        return false;
    }
    if (_pet == nullptr) {
        LogError("Open water ET process requires PET forcing.");
        return false;
    }

    return true;
}

void ProcessETOpenWater::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == VariableType::PET) {
        _pet = forcing;
    } else {
        throw ModelConfigError("Open water ET: forcing must be PET");
    }
}

const vecDouble& ProcessETOpenWater::GetRates() {
    // Evaporate at the potential rate, but never more than the available content over the
    // time step, so the store cannot go negative. (The container's constraints also cap
    // outflows, but in the explicit/direct computation path the static inflows are folded
    // into the content before the cap is evaluated, so the cap alone can let a small store
    // overshoot; capping here makes the process robust on both paths.)
    double rate = _pet->GetValue();
    if (_timeMachine != nullptr) {
        double timeStepInDays = *_timeMachine->GetTimeStepPointer();
        if (timeStepInDays > 0) {
            double maxRate = _container->GetContentWithChanges() / timeStepInDays;
            rate = std::min(rate, std::max(0.0, maxRate));
        }
    }

    return StoreRates({rate});
}
