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

    void ApplyConstraints(double timeStep) override;

    void Finalize() override;

  protected:

  private:
};

#endif  // HYDROBRICKS_GLACIER_H
