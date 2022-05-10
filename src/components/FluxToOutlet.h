#ifndef HYDROBRICKS_FLUX_TO_OUTLET_H
#define HYDROBRICKS_FLUX_TO_OUTLET_H

#include "Includes.h"
#include "Flux.h"

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

  protected:

  private:
};

#endif  // HYDROBRICKS_FLUX_TO_OUTLET_H
