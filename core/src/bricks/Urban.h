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

    /**
     * @copydoc Brick::ApplyConstraints()
     */
    void ApplyConstraints(double timeStep) override;

    /**
     * @copydoc Brick::Finalize()
     */
    void Finalize() override;
};

#endif  // HYDROBRICKS_URBAN_H
