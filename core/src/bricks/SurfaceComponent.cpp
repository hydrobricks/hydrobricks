#include "SurfaceComponent.h"

#include "HydroUnit.h"

SurfaceComponent::SurfaceComponent(LandCover* parent)
    : Brick(),
      m_parent(parent),
      m_areaFraction(1.0) {
    m_needsSolver = false;
}

void SurfaceComponent::SetAreaFraction(double value) {
    m_areaFraction = value;
    for (auto process : m_processes) {
        for (auto output : process->GetOutputFluxes()) {
            if (output->NeedsWeighting()) {
                output->MultiplyFraction(value);
            }
        }
    }
}
