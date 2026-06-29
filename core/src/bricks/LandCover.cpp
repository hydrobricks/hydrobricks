#include "LandCover.h"

#include "HydroUnit.h"
#include "SurfaceComponent.h"

LandCover::LandCover()
    : Brick(),
      _areaFraction(1.0),
      _initialAreaFraction(1.0) {
    _needsSolver = false;
}

void LandCover::Reset() {
    Brick::Reset();
    // Restore the extent (area fraction) to the initial one captured at build time, so
    // land-cover-change actions (e.g. glacier evolution) are rolled back and the model
    // can be re-run from the same extent in a calibration loop.
    SetAreaFraction(_initialAreaFraction);
}

void LandCover::SaveAsInitialState() {
    Brick::SaveAsInitialState();
    // Adopt the current extent as the initial one to restore on later resets.
    _initialAreaFraction = _areaFraction;
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
