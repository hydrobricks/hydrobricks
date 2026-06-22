#include "InterceptionStorage.h"

InterceptionStorage::InterceptionStorage()
    : SurfaceComponent() {}

void InterceptionStorage::SetParameters(const BrickSettings& brickSettings) {
    Brick::SetParameters(brickSettings);
}
