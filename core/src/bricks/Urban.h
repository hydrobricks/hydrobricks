#ifndef HYDROBRICKS_URBAN_H
#define HYDROBRICKS_URBAN_H

#include "Includes.h"
#include "LandCover.h"

class Urban : public LandCover {
  public:
    Urban();

    /**
     * @copydoc Brick::SetParameters()
     */
    void SetParameters(const BrickSettings& brickSettings) override;

    void ApplyConstraints(double timeStep) override;

    void Finalize() override;

  protected:
  private:
};

#endif  // HYDROBRICKS_URBAN_H
