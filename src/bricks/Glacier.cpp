#include "Glacier.h"

Glacier::Glacier(HydroUnit *hydroUnit)
    : SurfaceComponent(hydroUnit, false),
      m_unlimitedSupply(true)
{}

void Glacier::AssignParameters(const BrickSettings &brickSettings) {
    Brick::AssignParameters(brickSettings);
    if (HasParameter(brickSettings, "unlimitedSupply")) {
        m_unlimitedSupply = GetParameterValuePointer(brickSettings, "unlimitedSupply");
    }
}

void Glacier::ApplyConstraints(double timeStep) {
    if (m_unlimitedSupply) {
        return;
    } else {
        Brick::ApplyConstraints(timeStep);
    }
}

void Glacier::Finalize() {
    if (m_unlimitedSupply) {
        return;
    } else {
        Brick::Finalize();
    }
}