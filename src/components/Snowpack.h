
#ifndef HYDROBRICKS_SNOWPACK_H
#define HYDROBRICKS_SNOWPACK_H

#include "Includes.h"
#include "Container.h"

class Snowpack : public Container {
  public:
    Snowpack(HydroUnit *hydroUnit);

    ~Snowpack() override = default;

  protected:

  private:
};

#endif  // HYDROBRICKS_SNOWPACK_H
