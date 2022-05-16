#include "Glacier.h"

Glacier::Glacier(HydroUnit *hydroUnit)
    : Brick(hydroUnit)
{}

void Glacier::AssignParameters(const BrickSettings &brickSettings) {
    Brick::AssignParameters(brickSettings);
}

vecDouble Glacier::ComputeOutputs() {
    return {};
}