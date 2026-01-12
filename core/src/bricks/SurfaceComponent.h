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
    [[nodiscard]] bool CanHaveAreaFraction() const override {
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
    double GetParentAreaFraction() const;

    /**
     * @copydoc Brick::IsNull()
     */
    [[nodiscard]] bool IsNull() const override;

    /**
     * Check if the component has a parent land cover.
     *
     * @return true if the component has a parent land cover.
     */
    [[nodiscard]] bool HasParent() const {
        return _parent != nullptr;
    }

    /**
     * Get the parent land cover of this component.
     *
     * @return The parent land cover of this component.
     */
    LandCover* GetParent() const {
        return _parent;
    }

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
