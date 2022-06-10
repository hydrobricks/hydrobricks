#include "Urban.h"

Urban::Urban()
    : SurfaceComponent()
{}

void Urban::AssignParameters(const BrickSettings &brickSettings) {
    Brick::AssignParameters(brickSettings);
}

void Urban::ApplyConstraints(double timeStep) {
    Brick::ApplyConstraints(timeStep);
}

void Urban::Finalize() {
    Brick::Finalize();
}