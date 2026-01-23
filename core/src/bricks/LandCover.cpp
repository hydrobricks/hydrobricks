#include "LandCover.h"

#include "HydroUnit.h"
#include "SurfaceComponent.h"

LandCover::LandCover()
    : Brick(),
      _areaFraction(1.0) {
    _needsSolver = false;
}

void LandCover::SetAreaFraction(double value) {
    _areaFraction = value;
    for (const auto& process : _processes) {
        for (int i = 0; i < process->GetOutputFluxCount(); ++i) {
            Flux* output = process->GetOutputFlux(i);
            if (output->NeedsWeighting()) {
                output->SetFractionLandCover(value);
            }
        }
    }
}

void LandCover::SurfaceComponentAdded(SurfaceComponent*) {
    // Nothing to do.
}
