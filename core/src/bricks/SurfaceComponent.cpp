#include "SurfaceComponent.h"

#include "HydroUnit.h"
#include "LandCover.h"

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
                output->SetFractionLandCover(value);
            }
        }
    }
}

bool SurfaceComponent::IsNull() {
    if (m_areaFraction <= PRECISION) {
        return true;
    }
    if (m_parent && m_parent->IsNull()) {
        return true;
    }

    return false;
}