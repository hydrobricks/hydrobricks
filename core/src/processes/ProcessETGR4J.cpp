#include "ProcessETGR4J.h"

#include <cmath>

#include "Brick.h"
#include "WaterContainer.h"

ProcessETGR4J::ProcessETGR4J(WaterContainer* container)
    : ProcessET(container),
      _pet(nullptr) {}

void ProcessETGR4J::RegisterProcessParametersAndForcing(SettingsModel* modelSettings) {
    modelSettings->AddProcessForcing("pet");
}

bool ProcessETGR4J::IsValid() const {
    if (!ProcessET::IsValid()) {
        return false;
    }
    if (_pet == nullptr) {
        LogError("GR4J ET process requires PET forcing.");
        return false;
    }

    return true;
}

void ProcessETGR4J::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == VariableType::PET) {
        _pet = forcing;
    } else {
        throw ModelConfigError("GR4J ET: forcing must be of type PET.");
    }
}

vecDouble ProcessETGR4J::GetRates() {
    double En = _pet->GetValue();  // En = max(0, E - P_total), updated by interception:gr4j

    if (En <= 0) {
        return {0};
    }

    double X1 = _container->GetMaximumCapacity();
    if (X1 <= 0) {
        return {0};
    }

    double S = _container->GetContentWithChanges();
    double Sr = S / X1;
    double WS = std::min(En / X1, 13.0);  // 13: value at which tanh(WS) is close to 1 (num. stability cap)
    double TWS = std::tanh(WS);
    double Es = S * (2.0 - Sr) * TWS / (1.0 + (1.0 - Sr) * TWS);

    return {Es};
}
