#include "ProcessInterceptionGR4J.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessInterceptionGR4J::ProcessInterceptionGR4J(WaterContainer* container)
    : ProcessET(container),
      _pet(nullptr) {}

void ProcessInterceptionGR4J::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessForcing("pet");
}

bool ProcessInterceptionGR4J::IsValid() const {
    if (!ProcessET::IsValid()) {
        return false;
    }
    if (_pet == nullptr) {
        LogError("GR4J interception process requires PET forcing.");
        return false;
    }

    return true;
}

void ProcessInterceptionGR4J::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == VariableType::PET) {
        _pet = forcing;
    } else {
        throw ModelConfigError("GR4J interception: forcing must be of type PET.");
    }
}

const vecDouble& ProcessInterceptionGR4J::GetRates() {
    double P = _container->GetContentWithChanges();  // P_total (rain + melt)
    double E = _pet->GetValue();
    _pet->UpdateValue(std::max(0.0, E - P));  // publish En for downstream ET processes
    return StoreRates({std::min(P, E)});
}
