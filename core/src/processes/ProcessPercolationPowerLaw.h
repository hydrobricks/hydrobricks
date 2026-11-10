#ifndef HYDROBRICKS_PROCESS_PERCOLATION_POWER_LAW_H
#define HYDROBRICKS_PROCESS_PERCOLATION_POWER_LAW_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * Percolation following the filling of the store.
 *
 *   outflow = perc_max * (S / S_max)^exponent    [mm/d]
 *
 * The rate is the maximum one when the store is full and falls to zero as it dries:
 * the recharge follows the wetness of the store, so a lower store it feeds follows
 * the wet and dry years, instead of the constant source of a constant percolation.
 * Requires a store with a maximum capacity.
 */
class ProcessPercolationPowerLaw : public ProcessOutflow {
  public:
    explicit ProcessPercolationPowerLaw(WaterContainer* container);

    ~ProcessPercolationPowerLaw() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessSettings(SettingsModel* modelSettings);

    /**
     * @copydoc Process::IsValid()
     */
    [[nodiscard]] bool IsValid() const override;

    /**
     * @copydoc Process::SetParameters()
     */
    void SetParameters(const ProcessSettings& processSettings) override;

  protected:
    const float* _rate;      // [mm/d] at full store
    const float* _exponent;  // [-]

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_PERCOLATION_POWER_LAW_H
