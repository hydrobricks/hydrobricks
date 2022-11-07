#ifndef HYDROBRICKS_SURFACE_COMPONENT_H
#define HYDROBRICKS_SURFACE_COMPONENT_H

#include "Brick.h"
#include "Includes.h"

class HydroUnit;

class BaseSurfaceComponent : public Brick {
  public:
    BaseSurfaceComponent();

    bool CanHaveAreaFraction() override {
        return true;
    }

    double GetAreaFraction() {
        return m_areaFraction;
    }

    void SetAreaFraction(double value);

    bool IsNull() override {
        return m_areaFraction <= PRECISION;
    }

  protected:
    double m_areaFraction;

  private:
};

#endif  // HYDROBRICKS_SURFACE_COMPONENT_H
