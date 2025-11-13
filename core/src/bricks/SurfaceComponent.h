#ifndef HYDROBRICKS_SURFACE_COMPONENT_H
#define HYDROBRICKS_SURFACE_COMPONENT_H

#include "Brick.h"
#include "Includes.h"
#include "LandCover.h"

class HydroUnit;

class SurfaceComponent : public Brick {
  public:
    SurfaceComponent();

    /**
     * @copydoc Brick::CanHaveAreaFraction()
     */
    [[nodiscard]] bool CanHaveAreaFraction() override {
        return true;
    }

    /**
     * Gets the area fraction of this component.
     *
     * @return The area fraction of this component.
     */
    double GetAreaFraction() const {
        return _areaFraction;
    }

    /**
     * Sets the area fraction of this component.
     *
     * @param value The area fraction of this component.
     */
    void SetAreaFraction(double value);

    /**
     * Gets the area fraction of the parent land cover of this component.
     *
     * @return The area fraction of the parent land cover.
     */
    double GetParentAreaFraction();

    /**
     * @copydoc Brick::IsNull()
     */
    [[nodiscard]] bool IsNull() override;

    /**
     * Sets the parent land cover of this component.
     *
     * @param parent The parent land cover of this component.
     */
    virtual void SetParent(LandCover* parent) {
        _parent = parent;
        _parent->SurfaceComponentAdded(this);
    }

  protected:
    LandCover* _parent;
    double _areaFraction;
};

#endif  // HYDROBRICKS_SURFACE_COMPONENT_H
