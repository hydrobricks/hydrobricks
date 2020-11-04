
#ifndef FLHY_FLUX_LATERAL_H
#define FLHY_FLUX_LATERAL_H

#include "Includes.h"
#include "Flux.h"

class FluxLateral : public Flux {
  public:
    explicit FluxLateral();

    ~FluxLateral() override = default;

  protected:

  private:
};

#endif  // FLHY_FLUX_LATERAL_H
