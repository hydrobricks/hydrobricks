#include "ProcessInfiltrationGR4J.h"

#include "WaterContainer.h"
#include "helpers/GR4JFormulas.h"

ProcessInfiltrationGR4J::ProcessInfiltrationGR4J(WaterContainer* container)
    : ProcessInfiltration(container) {}

void ProcessInfiltrationGR4J::RegisterProcessSettings(SettingsModel* /*modelSettings*/) {}

vecDouble ProcessInfiltrationGR4J::GetRates() {
    double Pn = _container->GetContentWithChanges();  // net precipitation from ground_soil
    double X1 = GetTargetCapacity();                  // production store capacity
    if (Pn <= 0 || X1 <= 0) {
        return {0};
    }

    // Ps: portion of Pn entering the production store (current store level GetTargetStock()).
    return {gr4j::Infiltration(GetTargetStock(), Pn, X1)};
}
