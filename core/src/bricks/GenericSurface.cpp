#include "GenericSurface.h"

GenericSurface::GenericSurface()
    : SurfaceComponent()
{}

void GenericSurface::AssignParameters(const BrickSettings &brickSettings) {
    Brick::AssignParameters(brickSettings);
}

void GenericSurface::ApplyConstraints(double timeStep, bool inSolver) {
    Brick::ApplyConstraints(timeStep, inSolver);
}

void GenericSurface::Finalize() {
    Brick::Finalize();
}