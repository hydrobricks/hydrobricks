#include "SurfaceComponent.h"
#include "HydroUnit.h"

SurfaceComponent::SurfaceComponent(HydroUnit* hydroUnit, bool withWaterContainer)
    : Brick(hydroUnit, withWaterContainer),
      m_areaRatio(0)
{}
