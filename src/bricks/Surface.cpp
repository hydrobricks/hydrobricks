#include "Surface.h"

Surface::Surface(HydroUnit *hydroUnit)
    : Brick(hydroUnit),
      m_waterHeight(0)
{}

void Surface::AssignParameters(const BrickSettings &brickSettings) {
    Brick::AssignParameters(brickSettings);
}
