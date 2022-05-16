#include "Snowpack.h"

Snowpack::Snowpack(HydroUnit *hydroUnit)
    : Brick(hydroUnit)
{}

void Snowpack::AssignParameters(const BrickSettings &brickSettings) {
    Brick::AssignParameters(brickSettings);
}
