#ifndef HYDROBRICKS_BASE_LAND_COVER_H
#define HYDROBRICKS_BASE_LAND_COVER_H

#include "Brick.h"
#include "Includes.h"
#include "SurfaceComponent.h"

class HydroUnit;

class LandCover : public Brick {
  public:
    LandCover();

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

#endif  // HYDROBRICKS_BASE_LAND_COVER_H
