#include "ProcessETGR4J.h"

#include "WaterContainer.h"
#include "helpers/GR4JFormulas.h"

ProcessETGR4J::ProcessETGR4J(WaterContainer* container)
    : ProcessET(container),
      _pet(nullptr) {}

void ProcessETGR4J::RegisterProcessSettings(SettingsModel* modelSettings) {
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

const vecDouble& ProcessETGR4J::GetRates() {
    double En = _pet->GetValue();  // En = max(0, E - P_total), updated by interception:gr4j

    if (En <= 0) {
        return StoreRates({0});
    }

    double X1 = _container->GetMaximumCapacity();
    if (X1 <= 0) {
        return StoreRates({0});
    }

    // Evaporation is drawn from the store after this step's infiltration, using
    // the start-of-step level so the value is identical at every solver stage.
    // (Pn and En are mutually exclusive, so Ps is zero whenever En > 0; computing
    // it keeps the production and ET processes consistent in all cases.)
    double S0 = _container->GetContentWithoutChanges();
    double Pn = _container->GetContentWithChanges() - _container->GetContentWithDynamicChanges();
    double S1 = S0 + gr4j::Infiltration(S0, Pn, X1);

    return StoreRates({gr4j::Evaporation(S1, En, X1)});
}
