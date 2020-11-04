
#ifndef FLHY_FLUX_VERTICAL_H
#define FLHY_FLUX_VERTICAL_H

#include "Includes.h"
#include "Flux.h"

class FluxVertical : public Flux {
  public:
    explicit FluxVertical();

    ~FluxVertical() override = default;

  protected:

  private:
};

#endif  // FLHY_FLUX_VERTICAL_H
