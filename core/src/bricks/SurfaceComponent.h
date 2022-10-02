#ifndef HYDROBRICKS_SURFACE_COMPONENT_H
#define HYDROBRICKS_SURFACE_COMPONENT_H

#include "Brick.h"
#include "Includes.h"

class HydroUnit;

class SurfaceComponent : public Brick {
  public:
    SurfaceComponent();

    bool IsSurfaceComponent() override {
        return true;
    }

    double GetAreaFraction() {
        return m_areaFraction;
    }

    void SetAreaFraction(double value);

    bool IsNull() override {
        return m_areaFraction <= PRECISION;
    }

    virtual void AddToRelatedBricks(SurfaceComponent* brick) {
        m_relatedBricks.push_back(brick);
    }

  protected:
    double m_areaFraction;
    std::vector<SurfaceComponent*> m_relatedBricks;

  private:
};

#endif  // HYDROBRICKS_SURFACE_COMPONENT_H
