#ifndef HYDROBRICKS_URBAN_H
#define HYDROBRICKS_URBAN_H

#include "SurfaceComponent.h"
#include "Includes.h"

class Urban : public SurfaceComponent {
  public:
    Urban();

    /**
     * @copydoc Brick::AssignParameters()
     */
    void AssignParameters(const BrickSettings &brickSettings) override;

    void ApplyConstraints(double timeStep, bool inSolver = true) override;

    void Finalize() override;

  protected:

  private:
};

#endif  // HYDROBRICKS_URBAN_H
