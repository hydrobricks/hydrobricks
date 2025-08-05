#ifndef HYDROBRICKS_FLUX_TO_OUTLET_H
#define HYDROBRICKS_FLUX_TO_OUTLET_H

#include "Flux.h"
#include "Includes.h"

class FluxToOutlet : public Flux {
  public:
    explicit FluxToOutlet();

    /**
     * @copydoc Flux::IsOk()
     */
    bool IsOk() override;

    /**
     * @copydoc Flux::GetAmount()
     */
    double GetAmount() override;
};

#endif  // HYDROBRICKS_FLUX_TO_OUTLET_H
