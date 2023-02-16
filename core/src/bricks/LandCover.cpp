#include "LandCover.h"

#include "HydroUnit.h"
#include "SurfaceComponent.h"

LandCover::LandCover()
    : Brick(),
      m_areaFraction(1.0) {
    m_needsSolver = false;
}

void LandCover::SetAreaFraction(double value) {
    m_areaFraction = value;
    for (auto process : m_processes) {
        for (auto output : process->GetOutputFluxes()) {
            if (output->NeedsWeighting()) {
                output->SetFractionLandCover(value);
            }
        }
    }
}

void LandCover::SurfaceComponentAdded(SurfaceComponent*) {
    // Nothing to do.
}