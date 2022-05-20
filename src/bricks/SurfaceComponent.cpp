#include "SurfaceComponent.h"
#include "HydroUnit.h"

SurfaceComponent::SurfaceComponent(HydroUnit* hydroUnit, bool withWaterContainer)
    : Brick(hydroUnit, withWaterContainer),
      m_areaRatio(1.0)
{
    m_needsSolver = false;
}

void SurfaceComponent::SetAreaRatio(double value) {
    m_areaRatio = value;
    for (auto brick : m_relatedBricks) {
        brick->SetAreaRatio(value);
    }
}