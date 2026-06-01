#include "ProcessPercolationGR4J.h"

#include <cmath>

#include "Brick.h"
#include "WaterContainer.h"

ProcessPercolationGR4J::ProcessPercolationGR4J(WaterContainer* container)
    : ProcessOutflow(container) {}

void ProcessPercolationGR4J::RegisterProcessSettings(SettingsModel*) {
    // No parameters or forcing: X1 is read from the container's maximum capacity.
}

vecDouble ProcessPercolationGR4J::GetRates() {
    double X1 = _container->GetMaximumCapacity();
    if (X1 <= 0) {
        return {0};
    }

    double S = _container->GetContentWithChanges();
    double ratio = S / X1;
    // Perc = S × (1 − (1 + (4/9 * S/X1)⁴)^(−1/4)) = S × (1 − (1 + (S/X1)⁴ / 25.62890625)^(−1/4))
    double Perc = S * (1.0 - std::pow(1.0 + (ratio * ratio * ratio * ratio) / 25.62890625, -0.25));

    return {Perc};
}
