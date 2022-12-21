#ifndef HYDROBRICKS_BASE_LAND_COVER_H
#define HYDROBRICKS_BASE_LAND_COVER_H

#include "Brick.h"
#include "Includes.h"

class HydroUnit;
class SurfaceComponent;

class LandCover : public Brick {
  public:
    LandCover();

    bool CanHaveAreaFraction() override {
        return true;
    }

    double GetAreaFraction() {
        return m_areaFraction;
    }

    double* GetAreaFractionPointer() {
        return &m_areaFraction;
    }

    void SetAreaFraction(double value);

    virtual void SurfaceComponentAdded(SurfaceComponent* brick);

    bool IsLandCover() override {
        return true;
    }

    bool IsNull() override {
        return m_areaFraction <= PRECISION;
    }

  protected:
    double m_areaFraction;

  private:
};

#endif  // HYDROBRICKS_BASE_LAND_COVER_H
