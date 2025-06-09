#ifndef HYDROBRICKS_BASE_LAND_COVER_H
#define HYDROBRICKS_BASE_LAND_COVER_H

#include "Brick.h"
#include "Includes.h"

class HydroUnit;
class SurfaceComponent;

class LandCover : public Brick {
  public:
    LandCover();

    /**
     * @copydoc Brick::CanHaveAreaFraction()
     */
    bool CanHaveAreaFraction() override {
        return true;
    }

    /**
     * Get the area fraction of the land cover.
     *
     * @return The area fraction of the land cover.
     */
    double GetAreaFraction() {
        return m_areaFraction;
    }

    /**
     * Get a pointer to the area fraction of the land cover.
     *
     * @return A pointer to the area fraction of the land cover.
     */
    double* GetAreaFractionPointer() {
        return &m_areaFraction;
    }

    /**
     * Set the area fraction of the land cover.
     *
     * @param value The area fraction to set.
     */
    void SetAreaFraction(double value);

    /**
     * @copydoc Brick::IsLandCover()
     */
    bool IsLandCover() override {
        return true;
    }

    /**
     * @copydoc Brick::IsNull()
     */
    bool IsNull() override {
        return m_areaFraction <= PRECISION;
    }

    /**
     * Called when a new surface component is added to the land cover.
     *
     * @param brick The surface component that was added.
     */
    virtual void SurfaceComponentAdded(SurfaceComponent* brick);

  protected:
    double m_areaFraction;
};

#endif  // HYDROBRICKS_BASE_LAND_COVER_H
