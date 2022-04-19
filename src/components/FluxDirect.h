#ifndef HYDROBRICKS_FLUX_DIRECT_H
#define HYDROBRICKS_FLUX_DIRECT_H

#include "Includes.h"
#include "Flux.h"

class FluxDirect : public Flux {
  public:
    explicit FluxDirect();

    /**
     * @copydoc Flux::IsOk()
     */
    bool IsOk() override;

    /**
     * @copydoc Flux::GetWaterAmount()
     */
    double GetWaterAmount() override;

  protected:

  private:
};

#endif  // HYDROBRICKS_FLUX_DIRECT_H
