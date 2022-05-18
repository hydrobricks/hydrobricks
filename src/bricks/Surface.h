#ifndef HYDROBRICKS_SURFACE_H
#define HYDROBRICKS_SURFACE_H

#include "Surface.h"
#include "Includes.h"

class Surface : public SurfaceComponent {
  public:
    Vegetation(HydroUnit *hydroUnit);

    /**
     * @copydoc Brick::AssignParameters()
     */
    void AssignParameters(const BrickSettings &brickSettings) override;

    void ApplyConstraints(double timeStep) override;

    void Finalize() override;

  protected:

  private:
};

#endif  // HYDROBRICKS_SURFACE_H
