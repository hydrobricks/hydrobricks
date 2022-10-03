#include "Vegetation.h"

Vegetation::Vegetation()
    : SurfaceComponent() {}

void Vegetation::AssignParameters(const BrickSettings &brickSettings) {
    Brick::AssignParameters(brickSettings);
}

void Vegetation::ApplyConstraints(double timeStep, bool inSolver) {
    Brick::ApplyConstraints(timeStep, inSolver);
}

void Vegetation::Finalize() {
    Brick::Finalize();
}