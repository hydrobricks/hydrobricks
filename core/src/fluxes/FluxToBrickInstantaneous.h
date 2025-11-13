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
    [[nodiscard]] bool IsOk() override;

    /**
     * @copydoc Flux::IsInstantaneous()
     */
    bool IsInstantaneous() override {
        return true;
    }

    /**
     * @copydoc Flux::GetAmount()
     */
    double GetAmount() override;

    /**
     * Get the real amount of water that has been transferred to the brick.
     *
     * @return The real amount of water that has been transferred to the brick.
     */
    double GetRealAmount();

    /**
     * @copydoc Flux::UpdateFlux()
     */
    void UpdateFlux(double amount) override;
};

#endif  // HYDROBRICKS_FLUX_TO_BRICK_INSTANTANEOUS_H
