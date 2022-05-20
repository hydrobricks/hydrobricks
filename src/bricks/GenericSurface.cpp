#include "GenericSurface.h"

GenericSurface::GenericSurface(HydroUnit *hydroUnit)
    : SurfaceComponent(hydroUnit, true)
{}

void GenericSurface::AssignParameters(const BrickSettings &brickSettings) {
    Brick::AssignParameters(brickSettings);
}

void GenericSurface::ApplyConstraints(double timeStep) {
    Brick::ApplyConstraints(timeStep);
}

void GenericSurface::Finalize() {
    Brick::Finalize();
}