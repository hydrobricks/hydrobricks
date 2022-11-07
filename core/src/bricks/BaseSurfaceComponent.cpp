#include "BaseSurfaceComponent.h"

#include "HydroUnit.h"

BaseSurfaceComponent::BaseSurfaceComponent()
    : Brick(),
      m_areaFraction(1.0) {
    m_needsSolver = false;
}

void BaseSurfaceComponent::SetAreaFraction(double value) {
    m_areaFraction = value;
    for (auto process : m_processes) {
        for (auto output : process->GetOutputFluxes()) {
            if (output->NeedsWeighting()) {
                output->MultiplyFraction(value);
            }
        }
    }
}
