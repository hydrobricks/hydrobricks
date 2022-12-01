#ifndef HYDROBRICKS_FLUX_TO_BRICK_INSTANTANEOUS_H
#define HYDROBRICKS_FLUX_TO_BRICK_INSTANTANEOUS_H

#include "FluxToBrick.h"
#include "Includes.h"

class Brick;

class FluxToBrickInstantaneous : public FluxToBrick {
  public:
    explicit FluxToBrickInstantaneous(Brick* brick);

    /**
     * @copydoc Flux::IsOk()
     */
    bool IsOk() override;

    bool IsInstantaneous() override {
        return true;
    }

    /**
     * @copydoc Flux::GetAmount()
     */
    double GetAmount() override;

    double GetRealAmount();

    void UpdateFlux(double amount) override;

  protected:
  private:
};

#endif  // HYDROBRICKS_FLUX_TO_BRICK_INSTANTANEOUS_H
