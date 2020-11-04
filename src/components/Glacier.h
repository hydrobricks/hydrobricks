
#ifndef FLHY_GLACIER_H
#define FLHY_GLACIER_H

#include "Includes.h"
#include "Container.h"

class Glacier : public Container {
  public:
    Glacier(HydroUnit *hydroUnit);

    ~Glacier() override = default;

  protected:

  private:
};

#endif  // FLHY_GLACIER_H
