#ifndef HYDROBRICKS_SURFACE_COMPONENT_H
#define HYDROBRICKS_SURFACE_COMPONENT_H

#include "Includes.h"
#include "Brick.h"

class HydroUnit;

class SurfaceComponent : public Brick {
  public:
    SurfaceComponent(HydroUnit* hydroUnit, bool withWaterContainer);

  protected:
    double m_areaRatio;

  private:
};

#endif  // HYDROBRICKS_SURFACE_COMPONENT_H
