#include "Surface.h"

Surface::Surface()
    : SurfaceComponent()
{}

void Surface::AssignParameters(const BrickSettings &brickSettings) {
    Brick::AssignParameters(brickSettings);
}

void Surface::ApplyConstraints(double timeStep) {
    Brick::ApplyConstraints(timeStep);
}

void Surface::Finalize() {
    Brick::Finalize();
}