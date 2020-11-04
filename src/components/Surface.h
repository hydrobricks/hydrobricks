
#ifndef FLHY_SURFACE_H
#define FLHY_SURFACE_H

#include "Includes.h"
#include "Container.h"

class Surface : public Container {
  public:
    Surface(HydroUnit *hydroUnit);

    ~Surface() override = default;

  protected:
    double m_waterHeight;

  private:
};

#endif  // FLHY_SURFACE_H
