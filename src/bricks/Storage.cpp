#include "Storage.h"

Storage::Storage(HydroUnit *hydroUnit)
    : Brick(hydroUnit, true)
{}

void Storage::AssignParameters(const BrickSettings &brickSettings) {
    Brick::AssignParameters(brickSettings);
}
