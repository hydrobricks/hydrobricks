#ifndef HYDROBRICKS_GLACIER_H
#define HYDROBRICKS_GLACIER_H

#include "SurfaceComponent.h"
#include "Includes.h"

class Glacier : public SurfaceComponent {
  public:
    Glacier(HydroUnit *hydroUnit);

    /**
     * @copydoc Brick::AssignParameters()
     */
    void AssignParameters(const BrickSettings &brickSettings) override;

    WaterContainer* GetIceContainer();

    void ApplyConstraints(double timeStep) override;

    void Finalize() override;

    bool IsGlacier() override {
        return true;
    }

  protected:
    WaterContainer* m_ice;

  private:
};

#endif  // HYDROBRICKS_GLACIER_H
