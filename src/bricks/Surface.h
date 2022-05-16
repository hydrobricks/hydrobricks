#ifndef HYDROBRICKS_SURFACE_H
#define HYDROBRICKS_SURFACE_H

#include "Brick.h"
#include "Includes.h"

class Surface : public Brick {
  public:
    Surface(HydroUnit *hydroUnit);

    /**
     * @copydoc Brick::AssignParameters()
     */
    void AssignParameters(const BrickSettings &brickSettings) override;

  protected:

  private:
};

#endif  // HYDROBRICKS_SURFACE_H
