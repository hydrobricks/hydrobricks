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

    /**
     * @copydoc Brick::ApplyConstraints()
     */
    void ApplyConstraints(double timeStep) override;

    /**
     * @copydoc Brick::Finalize()
     */
    void Finalize() override;
};

#endif  // HYDROBRICKS_VEGETATION_H
