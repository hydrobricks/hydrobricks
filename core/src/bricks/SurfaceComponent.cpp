#include "SurfaceComponent.h"

#include "HydroUnit.h"
#include "LandCover.h"

SurfaceComponent::SurfaceComponent()
    : Brick(),
      _parent(nullptr),
      _areaFraction(1.0) {
    _needsSolver = false;
}

void SurfaceComponent::SetAreaFraction(double value) {
    wxASSERT(_parent);
    _areaFraction = value;
    for (auto process : _processes) {
        for (auto output : process->GetOutputFluxes()) {
            if (output->NeedsWeighting()) {
                value *= _parent->GetAreaFraction();
                output->SetFractionLandCover(value);
            }
        }
    }
}

bool SurfaceComponent::IsNull() {
    if (_areaFraction <= PRECISION) {
        return true;
    }
    if (_parent && _parent->IsNull()) {
        return true;
    }

    return false;
}