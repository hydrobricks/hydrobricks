#ifndef HYDROBRICKS_FLUX_SIMPLE_H
#define HYDROBRICKS_FLUX_SIMPLE_H

#include "Flux.h"
#include "Includes.h"

class FluxSimple : public Flux {
  public:
    explicit FluxSimple();

    /**
     * @copydoc Flux::IsOk()
     */
    bool IsOk() override;

    /**
     * @copydoc Flux::GetAmount()
     */
    double GetAmount() override;

    void UpdateFlux(double amount) override;

  protected:
  private:
};

#endif  // HYDROBRICKS_FLUX_SIMPLE_H
