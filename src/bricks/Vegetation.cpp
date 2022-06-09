#include "Vegetation.h"

Vegetation::Vegetation()
    : SurfaceComponent()
{}

void Vegetation::AssignParameters(const BrickSettings &brickSettings) {
    Brick::AssignParameters(brickSettings);
}

void Vegetation::ApplyConstraints(double timeStep) {
    Brick::ApplyConstraints(timeStep);
}

void Vegetation::Finalize() {
    Brick::Finalize();
}