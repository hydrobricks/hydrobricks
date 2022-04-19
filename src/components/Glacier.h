#ifndef HYDROBRICKS_GLACIER_H
#define HYDROBRICKS_GLACIER_H

#include "Brick.h"
#include "Includes.h"

class Glacier : public Brick {
  public:
    Glacier(HydroUnit *hydroUnit);

    ~Glacier() override = default;

  protected:

  private:
};

#endif  // HYDROBRICKS_GLACIER_H
