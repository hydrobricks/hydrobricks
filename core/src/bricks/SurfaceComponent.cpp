#include "SurfaceComponent.h"

#include "HydroUnit.h"

SurfaceComponent::SurfaceComponent()
    : Brick(),
      m_parent(nullptr),
      m_areaFraction(1.0) {
    m_needsSolver = false;
}

void SurfaceComponent::SetAreaFraction(double value) {
    wxASSERT(m_parent);
    m_areaFraction = value;
    for (auto process : m_processes) {
        for (auto output : process->GetOutputFluxes()) {
            if (output->NeedsWeighting()) {
                value *= m_parent->GetAreaFraction();
                output->MultiplyFraction(value);
            }
        }
    }
}
