#include "SurfaceComponent.h"

SurfaceComponent::SurfaceComponent(HydroUnit* hydroUnit, bool withWaterContainer)
    : Brick(hydroUnit, withWaterContainer),
      m_areaRatio(0)
{}
