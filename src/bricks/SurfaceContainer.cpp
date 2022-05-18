#include "SurfaceContainer.h"

SurfaceContainer::SurfaceContainer(HydroUnit *hydroUnit)
    : Brick(hydroUnit, false)
{}

void SurfaceContainer::AssignParameters(const BrickSettings &brickSettings) {
    Brick::AssignParameters(brickSettings);
}
