
#ifndef HYDROBRICKS_FLUX_LATERAL_H
#define HYDROBRICKS_FLUX_LATERAL_H

#include "Includes.h"
#include "Flux.h"

class FluxLateral : public Flux {
  public:
    explicit FluxLateral();

    ~FluxLateral() override = default;

  protected:

  private:
};

#endif  // HYDROBRICKS_FLUX_LATERAL_H
