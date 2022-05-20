#include "Surface.h"

Surface::Surface(HydroUnit *hydroUnit)
    : SurfaceComponent(hydroUnit, true)
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