#include "ProcessLateral.h"

#include "Brick.h"
#include "FluxToBrick.h"
#include "HydroUnit.h"
#include "SurfaceComponent.h"

ProcessLateral::ProcessLateral(WaterContainer* container)
    : Process(container) {}

bool ProcessLateral::IsValid() const {
    if (_outputs.size() == 0) {
        LogError("Lateral processes need at least 1 connection.");
        return false;
    }

    return true;
}

int ProcessLateral::GetConnectionCount() const {
    return static_cast<int>(_outputs.size());
}

double* ProcessLateral::GetValuePointer(const string& name) {
    // Parse the name to get the connection index (output_i).
    if (name.substr(0, 7) == "output_") {
        int index = std::stoi(name.substr(7));
        if (index < 0 || index >= static_cast<int>(_outputs.size())) {
            LogError("Invalid output index: {}", index);
            return nullptr;
        }
        return _outputs[index]->GetAmountPointer();
    }

    return nullptr;
}

void ProcessLateral::AttachFluxOutWithWeight(std::unique_ptr<Flux> flux, double weight) {
    assert(flux);
    _outputs.push_back(std::move(flux));
    _weights.push_back(weight);
}

double ProcessLateral::GetOriginLandCoverAreaFraction() const {
    Brick* brick = _container->GetParentBrick();
    assert(brick);
    auto surfaceComponent = dynamic_cast<SurfaceComponent*>(brick);
    assert(surfaceComponent);

    return surfaceComponent->GetParentAreaFraction();
}

double ProcessLateral::GetTargetLandCoverAreaFraction(Flux* flux) {
    FluxToBrick* fluxToBrick = dynamic_cast<FluxToBrick*>(flux);
    assert(flux);
    Brick* targetBrick = fluxToBrick->GetTargetBrick();
    assert(targetBrick);
    auto surfaceComponent = dynamic_cast<SurfaceComponent*>(targetBrick);
    assert(surfaceComponent);

    return surfaceComponent->GetParentAreaFraction();
}

double ProcessLateral::ComputeFractionAreas(Flux* flux) {
    auto fluxToBrick = dynamic_cast<FluxToBrick*>(flux);
    assert(fluxToBrick);

    double destinationArea = fluxToBrick->GetTargetBrick()->GetHydroUnit()->GetArea();
    double originArea = _container->GetParentBrick()->GetHydroUnit()->GetArea();

    return (originArea * GetOriginLandCoverAreaFraction()) / (destinationArea * GetTargetLandCoverAreaFraction(flux));
}
