#include "ProcessProductionGR4J.h"

#include "FormulasGR4J.h"
#include "WaterContainer.h"

ProcessProductionGR4J::ProcessProductionGR4J(WaterContainer* container)
    : ProcessOutflow(container),
      _pet(nullptr) {}

void ProcessProductionGR4J::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessForcing("pet");
}

bool ProcessProductionGR4J::IsValid() const {
    if (!ProcessOutflow::IsValid()) {
        return false;
    }
    if (_pet == nullptr) {
        LogError("GR4J production process requires PET forcing.");
        return false;
    }

    return true;
}

void ProcessProductionGR4J::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == VariableType::PET) {
        _pet = forcing;
    } else {
        throw ModelConfigError("GR4J production: forcing must be of type PET.");
    }
}

vecDouble ProcessProductionGR4J::GetRates() {
    if (_pet == nullptr) {
        return {0};
    }

    double X1 = _container->GetMaximumCapacity();
    if (X1 <= 0) {
        return {0};
    }

    // Start-of-step store level and this step's net precipitation. Pn is the
    // instantaneous throughfall, held in the static content change; both are
    // constant across solver stages, so the discrete result is solver-independent.
    double S0 = _container->GetContentWithoutChanges();
    double Pn = _container->GetContentWithChanges() - _container->GetContentWithDynamicChanges();
    double En = _pet->GetValue();  // net evaporative demand, published by interception:gr4j

    // GR4J production sequence: infiltration, then evaporation, then percolation.
    double Ps = gr4j::Infiltration(S0, Pn, X1);
    double S1 = S0 + Ps;
    double Es = gr4j::Evaporation(S1, En, X1);  // recomputed here only to get S for percolation
    double S2 = S1 - Es;
    double Perc = gr4j::Percolation(S2, X1);

    // Water routed to the unit hydrograph input: the part of Pn that did not
    // infiltrate, plus percolation leaving the store.
    double PR = (Pn - Ps) + Perc;

    return {PR};
}
