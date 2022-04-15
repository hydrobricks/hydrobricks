
#ifndef HYDROBRICKS_SURFACE_H
#define HYDROBRICKS_SURFACE_H

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

#endif  // HYDROBRICKS_SURFACE_H
