#ifndef HYDROBRICKS_SURFACE_H
#define HYDROBRICKS_SURFACE_H

#include "Brick.h"
#include "Includes.h"

class Surface : public Brick {
  public:
    Surface(HydroUnit *hydroUnit);

    ~Surface() override = default;

  protected:
    double m_waterHeight;

  private:
};

#endif  // HYDROBRICKS_SURFACE_H
