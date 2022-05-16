#ifndef HYDROBRICKS_GLACIER_H
#define HYDROBRICKS_GLACIER_H

#include "Brick.h"
#include "Includes.h"

class Glacier : public Brick {
  public:
    Glacier(HydroUnit *hydroUnit);

    /**
     * @copydoc Brick::AssignParameters()
     */
    void AssignParameters(const BrickSettings &brickSettings) override;

  protected:

    /**
     * @copydoc Brick::ComputeOutputs()
     */
    vecDouble ComputeOutputs() override;

  private:
};

#endif  // HYDROBRICKS_GLACIER_H
