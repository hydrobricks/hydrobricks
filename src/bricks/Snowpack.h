#ifndef HYDROBRICKS_SNOWPACK_H
#define HYDROBRICKS_SNOWPACK_H

#include "Includes.h"
#include "SurfaceComponent.h"

class Snowpack : public SurfaceComponent {
  public:
    Snowpack(HydroUnit *hydroUnit);

    /**
     * @copydoc Brick::AssignParameters()
     */
    void AssignParameters(const BrickSettings &brickSettings) override;

  protected:

  private:
};

#endif  // HYDROBRICKS_SNOWPACK_H
