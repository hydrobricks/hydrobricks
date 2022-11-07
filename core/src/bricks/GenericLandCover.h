#ifndef HYDROBRICKS_GENERIC_LAND_COVER_H
#define HYDROBRICKS_GENERIC_LAND_COVER_H

#include "BaseLandCover.h"
#include "Includes.h"

class GenericLandCover : public BaseLandCover {
  public:
    GenericLandCover();

    /**
     * @copydoc Brick::AssignParameters()
     */
    void AssignParameters(const BrickSettings& brickSettings) override;

    void ApplyConstraints(double timeStep, bool inSolver = true) override;

    void Finalize() override;

  protected:
  private:
};

#endif  // HYDROBRICKS_GENERIC_LAND_COVER_H
