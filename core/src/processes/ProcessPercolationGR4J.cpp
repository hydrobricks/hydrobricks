#include "ProcessPercolationGR4J.h"

#include "Brick.h"
#include "FormulasGR4J.h"
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

    return {gr4j::Percolation(_container->GetContentWithChanges(), X1)};
}
