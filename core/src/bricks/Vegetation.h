#ifndef HYDROBRICKS_VEGETATION_H
#define HYDROBRICKS_VEGETATION_H

#include "Includes.h"
#include "LandCover.h"

class Vegetation : public LandCover {
  public:
    Vegetation();

    /**
     * @copydoc Brick::SetParameters()
     */
    void SetParameters(const BrickSettings& brickSettings) override;

    void ApplyConstraints(double timeStep) override;

    void Finalize() override;

  protected:
  private:
};

#endif  // HYDROBRICKS_VEGETATION_H
