#include "ProcessInfiltrationGR4J.h"

#include <cmath>

#include "WaterContainer.h"

ProcessInfiltrationGR4J::ProcessInfiltrationGR4J(WaterContainer* container)
    : ProcessInfiltration(container) {}

void ProcessInfiltrationGR4J::RegisterProcessParametersAndForcing(SettingsModel* /*modelSettings*/) {}

vecDouble ProcessInfiltrationGR4J::GetRates() {
    double Pn = _container->GetContentWithChanges();  // net precipitation from ground_soil
    if (Pn <= 0) {
        return {0};
    }

    double X1 = GetTargetCapacity();
    if (X1 <= 0) {
        return {0};
    }

    double Sr = GetTargetFillingRatio();  // S / X1 from production store
    double WS = std::min(Pn / X1, 13.0);  // 13: value at which tanh(WS) is close to 1 (num. stability cap)
    double TWS = std::tanh(WS);
    double Ps = X1 * (1.0 - Sr * Sr) * TWS / (1.0 + Sr * TWS);

    return {Ps};
}
