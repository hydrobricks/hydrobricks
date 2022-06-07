#include "Glacier.h"

Glacier::Glacier(HydroUnit *hydroUnit)
    : SurfaceComponent(hydroUnit),
      m_ice(nullptr)
{
    m_ice = new WaterContainer(this);
}

void Glacier::AssignParameters(const BrickSettings &brickSettings) {
    Brick::AssignParameters(brickSettings);
}

WaterContainer* Glacier::GetIceContainer() {
    return m_ice;
}

void Glacier::ApplyConstraints(double) {
    // Nothing to do
}

void Glacier::Finalize() {
    // Nothing to do
}