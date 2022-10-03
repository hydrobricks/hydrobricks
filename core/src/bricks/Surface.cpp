#include "Surface.h"

Surface::Surface()
    : SurfaceComponent() {}

void Surface::AssignParameters(const BrickSettings& brickSettings) {
    Brick::AssignParameters(brickSettings);
}

void Surface::ApplyConstraints(double timeStep, bool inSolver) {
    Brick::ApplyConstraints(timeStep, inSolver);
}

void Surface::Finalize() {
    Brick::Finalize();
}