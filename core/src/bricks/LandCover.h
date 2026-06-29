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
     * @copydoc Brick::Reset()
     *
     * Also restores the area fraction to the initial extent (set at build time via
     * SetInitialAreaFraction), so that land-cover changes (e.g. glacier evolution) are
     * rolled back when the model is re-run.
     */
    void Reset() override;

    /**
     * @copydoc Brick::SaveAsInitialState()
     *
     * Also stores the current area fraction as the initial extent to restore on reset.
     */
    void SaveAsInitialState() override;

    /**
     * @copydoc Brick::CanHaveAreaFraction()
     */
    [[nodiscard]] bool CanHaveAreaFraction() const override {
        return true;
    }

    /**
     * Get the area fraction of the land cover.
     *
     * @return The area fraction of the land cover.
     */
    double GetAreaFraction() const {
        return _areaFraction;
    }

    /**
     * Get a pointer to the area fraction of the land cover.
     *
     * @return A pointer to the area fraction of the land cover.
     */
    double* GetAreaFractionPointer() {
        return &_areaFraction;
    }

    /**
     * Set the area fraction of the land cover.
     *
     * @param value The area fraction to set.
     */
    void SetAreaFraction(double value);

    /**
     * Set the initial area fraction (the extent restored on every reset). Set once at
     * build time, when the land cover fractions are assigned.
     *
     * @param value The initial area fraction to store.
     */
    void SetInitialAreaFraction(double value) {
        _initialAreaFraction = value;
    }

    /**
     * @copydoc Brick::IsLandCover()
     */
    [[nodiscard]] bool IsLandCover() const override {
        return true;
    }

    /**
     * @copydoc Brick::IsNull()
     */
    [[nodiscard]] bool IsNull() const override {
        return NearlyZero(_areaFraction, PRECISION);
    }

    /**
     * Called when a new surface component is added to the land cover.
     *
     * @param brick The surface component that was added.
     */
    virtual void SurfaceComponentAdded(SurfaceComponent* brick);

  protected:
    double _areaFraction;
    double _initialAreaFraction;
};

#endif  // HYDROBRICKS_BASE_LAND_COVER_H
