#ifndef HYDROBRICKS_VEGETATION_H
#define HYDROBRICKS_VEGETATION_H

#include "Includes.h"
#include "LandCover.h"

class Vegetation : public LandCover {
  public:
    Vegetation();

    /**
     * @copydoc Brick::AssignParameters()
     */
    void AssignParameters(const BrickSettings& brickSettings) override;

    void ApplyConstraints(double timeStep, bool inSolver = true) override;

    void Finalize() override;

  protected:
  private:
};

#endif  // HYDROBRICKS_VEGETATION_H
