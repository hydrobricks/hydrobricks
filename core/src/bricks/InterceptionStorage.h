#ifndef HYDROBRICKS_INTERCEPTION_STORAGE_H
#define HYDROBRICKS_INTERCEPTION_STORAGE_H

#include "Includes.h"
#include "SurfaceComponent.h"

/**
 * Interception storage (e.g. a forest canopy).
 *
 * A surface component (so its area follows the parent land cover fraction) that
 * holds water up to a capacity. It is meant to sit on the rain path upstream of
 * the snowpack: it evaporates at the potential rate (et:open_water) and releases
 * the excess over the capacity as throughfall (overflow). Being a surface
 * component, its content and ET weight by the parent cover's area fraction, so
 * the catchment water balance closes when the cover fraction is below one.
 */
class InterceptionStorage : public SurfaceComponent {
  public:
    InterceptionStorage();

    /**
     * @copydoc Brick::SetParameters()
     */
    void SetParameters(const BrickSettings& brickSettings) override;
};

#endif  // HYDROBRICKS_INTERCEPTION_STORAGE_H
