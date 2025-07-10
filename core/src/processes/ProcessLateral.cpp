#include "ProcessLateral.h"

#include "Brick.h"
#include "FluxToBrick.h"
#include "SurfaceComponent.h"
#include "HydroUnit.h"

ProcessLateral::ProcessLateral(WaterContainer* container)
    : Process(container) {}

bool ProcessLateral::IsOk() {
    if (_outputs.size() == 0) {
        wxLogError(_("Lateral processes need at least 1 connection."));
        return false;
    }

    return true;
}

int ProcessLateral::GetConnectionsNb() {
    return _outputs.size();
}

double* ProcessLateral::GetValuePointer(const string& name) {
    // Parse the name to get the connection index (output_i).
    if (name.substr(0, 7) == "output_") {
        int index = std::stoi(name.substr(7));
        if (index < 0 || index >= static_cast<int>(_outputs.size())) {
            wxLogError(_("Invalid output index: %d"), index);
            return nullptr;
        }
        return _outputs[index]->GetAmountPointer();
    }

    return nullptr;
}

void ProcessLateral::AttachFluxOutWithWeight(Flux* flux, double weight) {
    wxASSERT(flux);
    _outputs.push_back(flux);
    _weights.push_back(weight);
}

double ProcessLateral::GetOriginLandCoverAreaFraction() {
    Brick* brick = _container->GetParentBrick();
    wxASSERT(brick);
    auto surfaceComponent = dynamic_cast<SurfaceComponent*>(brick);
    wxASSERT(surfaceComponent);

    return surfaceComponent->GetParentAreaFraction();
}

double ProcessLateral::GetTargetLandCoverAreaFraction(Flux* flux) {
    FluxToBrick* fluxToBrick = dynamic_cast<FluxToBrick*>(flux);
    wxASSERT(flux);
    Brick* targetBrick = fluxToBrick->GetTargetBrick();
    wxASSERT(targetBrick);
    auto surfaceComponent = dynamic_cast<SurfaceComponent*>(targetBrick);
    wxASSERT(surfaceComponent);

    return surfaceComponent->GetParentAreaFraction();
}

double ProcessLateral::ComputeFractionAreas(Flux* flux) {
    auto fluxToBrick = dynamic_cast<FluxToBrick*>(flux);
    wxASSERT(fluxToBrick);

    double destinationArea = fluxToBrick->GetTargetBrick()->GetHydroUnit()->GetArea();
    double originArea = _container->GetParentBrick()->GetHydroUnit()->GetArea();

    return (originArea * GetOriginLandCoverAreaFraction()) /
        (destinationArea * GetTargetLandCoverAreaFraction(flux));
}
