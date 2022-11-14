#ifndef HYDROBRICKS_SURFACE_COMPONENT_H
#define HYDROBRICKS_SURFACE_COMPONENT_H

#include "Brick.h"
#include "Includes.h"
#include "LandCover.h"

class HydroUnit;

class SurfaceComponent : public Brick {
  public:
    SurfaceComponent();

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

    virtual void SetParent(LandCover* parent) {
        m_parent = parent;
        m_parent->SurfaceComponentAdded(this);
    }

  protected:
    LandCover* m_parent;
    double m_areaFraction;

  private:
};

#endif  // HYDROBRICKS_SURFACE_COMPONENT_H
