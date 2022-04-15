
#ifndef HYDROBRICKS_GLACIER_H
#define HYDROBRICKS_GLACIER_H

#include "Includes.h"
#include "Container.h"

class Glacier : public Container {
  public:
    Glacier(HydroUnit *hydroUnit);

    ~Glacier() override = default;

  protected:

  private:
};

#endif  // HYDROBRICKS_GLACIER_H
