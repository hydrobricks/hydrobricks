#include "Glacier.h"

Glacier::Glacier(HydroUnit *hydroUnit)
    : SurfaceComponent(hydroUnit, false)
{}

void Glacier::AssignParameters(const BrickSettings &brickSettings) {
    Brick::AssignParameters(brickSettings);
}

void Glacier::ApplyConstraints(double) {
    // Nothing to do
}

void Glacier::Finalize() {
    // Nothing to do
}