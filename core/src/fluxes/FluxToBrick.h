#ifndef HYDROBRICKS_FLUX_TO_BRICK_H
#define HYDROBRICKS_FLUX_TO_BRICK_H

#include "Flux.h"
#include "Includes.h"

class Brick;

class FluxToBrick : public Flux {
  public:
    explicit FluxToBrick(Brick* brick);

    /**
     * @copydoc Flux::IsOk()
     */
    bool IsOk() override;

    /**
     * @copydoc Flux::GetAmount()
     */
    double GetAmount() override;

    /**
     * @copydoc Flux::UpdateFlux()
     */
    void UpdateFlux(double amount) override;

  protected:
    Brick* m_toBrick;
};

#endif  // HYDROBRICKS_FLUX_TO_BRICK_H
