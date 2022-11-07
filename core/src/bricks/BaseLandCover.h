#ifndef HYDROBRICKS_BASE_LAND_COVER_H
#define HYDROBRICKS_BASE_LAND_COVER_H

#include "Brick.h"
#include "Includes.h"

class HydroUnit;

class BaseLandCover : public Brick {
  public:
    BaseLandCover();

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

    virtual void AddToRelatedBricks(SurfaceComponent* brick) {
        m_relatedBricks.push_back(brick);
    }

  protected:
    double m_areaFraction;
    std::vector<SurfaceComponent*> m_relatedBricks;

  private:
};

#endif  // HYDROBRICKS_BASE_LAND_COVER_H
