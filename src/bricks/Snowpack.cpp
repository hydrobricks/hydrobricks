#include "Snowpack.h"

Snowpack::Snowpack(HydroUnit *hydroUnit)
    : SurfaceComponent(hydroUnit, true)
{}

void Snowpack::AssignParameters(const BrickSettings &brickSettings) {
    Brick::AssignParameters(brickSettings);
}
