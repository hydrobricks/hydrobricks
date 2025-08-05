#ifndef HYDROBRICKS_FLUX_TO_ATMOSPHERE_H
#define HYDROBRICKS_FLUX_TO_ATMOSPHERE_H

#include "Flux.h"
#include "Includes.h"

class FluxToAtmosphere : public Flux {
  public:
    explicit FluxToAtmosphere();

    /**
     * @copydoc Flux::IsOk()
     */
    bool IsOk() override;

    /**
     * @copydoc Flux::GetAmount()
     */
    double GetAmount() override;
};

#endif  // HYDROBRICKS_FLUX_TO_ATMOSPHERE_H
