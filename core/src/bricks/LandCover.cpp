#include "LandCover.h"

#include "HydroUnit.h"
#include "SurfaceComponent.h"

LandCover::LandCover()
    : Brick(),
      _areaFraction(1.0),
      _initialAreaFraction(1.0),
      _initialAreaSaved(false) {
    _needsSolver = false;
}

void LandCover::Reset() {
    Brick::Reset();
    // The extent (area fraction) has no explicit initial-state setter (unlike the
    // water/ice content), so capture it on the first reset — done before the first
    // run, while the model is still in its initial, post-setup state — and restore it
    // on every later reset. This rolls back land-cover-change actions (e.g. glacier
    // evolution) so the model can be re-run from the same extent in a calibration loop.
    if (!_initialAreaSaved) {
        _initialAreaFraction = _areaFraction;
        _initialAreaSaved = true;
    } else {
        SetAreaFraction(_initialAreaFraction);
    }
}

void LandCover::SaveAsInitialState() {
    Brick::SaveAsInitialState();
    // Adopt the current extent as the initial one to restore on later resets.
    _initialAreaFraction = _areaFraction;
    _initialAreaSaved = true;
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
