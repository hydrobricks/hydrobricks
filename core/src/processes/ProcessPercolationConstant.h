#ifndef HYDROBRICKS_PROCESS_PERCOLATION_CONSTANT_H
#define HYDROBRICKS_PROCESS_PERCOLATION_CONSTANT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * Constant-rate percolation.
 *
 *   outflow = perc    [mm/d]
 *
 * A fixed percolation rate, constrained by the available content via the
 * container. Used e.g. for the HBV-96 percolation from the upper to the lower zone.
 */
class ProcessPercolationConstant : public ProcessOutflow {
  public:
    explicit ProcessPercolationConstant(WaterContainer* container);

    ~ProcessPercolationConstant() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessSettings(SettingsModel* modelSettings);

    /**
     * @copydoc Process::SetParameters()
     */
    void SetParameters(const ProcessSettings& processSettings) override;

  protected:
    const float* _rate;  // [mm/d]

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_PERCOLATION_CONSTANT_H
