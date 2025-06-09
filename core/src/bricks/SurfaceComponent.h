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
    bool CanHaveAreaFraction() override {
        return true;
    }

    /**
     * Gets the area fraction of this component.
     *
     * @return The area fraction of this component.
     */
    double GetAreaFraction() const {
        return m_areaFraction;
    }

    /**
     * Sets the area fraction of this component.
     *
     * @param value The area fraction of this component.
     */
    void SetAreaFraction(double value);

    /**
     * @copydoc Brick::IsNull()
     */
    bool IsNull() override;

    /**
     * Sets the parent land cover of this component.
     *
     * @param parent The parent land cover of this component.
     */
    virtual void SetParent(LandCover* parent) {
        m_parent = parent;
        m_parent->SurfaceComponentAdded(this);
    }

  protected:
    LandCover* m_parent;
    double m_areaFraction;
};

#endif  // HYDROBRICKS_SURFACE_COMPONENT_H
