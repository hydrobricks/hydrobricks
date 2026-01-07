#include "SurfaceComponent.h"

#include "HydroUnit.h"
#include "LandCover.h"

SurfaceComponent::SurfaceComponent()
    : _parent(nullptr),
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

double SurfaceComponent::GetParentAreaFraction() {
    wxASSERT(_parent);
    return _parent->GetAreaFraction();
}

bool SurfaceComponent::IsNull() {
    if (NearlyZero(_areaFraction, PRECISION)) {
        return true;
    }
    if (_parent && _parent->IsNull()) {
        return true;
    }

    return false;
}