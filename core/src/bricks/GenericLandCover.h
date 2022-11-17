#ifndef HYDROBRICKS_GENERIC_LAND_COVER_H
#define HYDROBRICKS_GENERIC_LAND_COVER_H

#include "Includes.h"
#include "LandCover.h"

class GenericLandCover : public LandCover {
  public:
    GenericLandCover();

    /**
     * @copydoc Brick::AssignParameters()
     */
    void AssignParameters(const BrickSettings& brickSettings) override;

    void ApplyConstraints(double timeStep) override;

    void Finalize() override;

  protected:
  private:
};

#endif  // HYDROBRICKS_GENERIC_LAND_COVER_H
