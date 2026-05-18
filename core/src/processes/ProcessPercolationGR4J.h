#ifndef HYDROBRICKS_PROCESS_PERCOLATION_GR4J_H
#define HYDROBRICKS_PROCESS_PERCOLATION_GR4J_H

#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * GR4J percolation from the production store.
 *
 *   Perc = S × (1 − (1 + (4/9 * S/X1)⁴)^(−1/4))
 *        = S × (1 − (1 + (S/X1)⁴ / 25.62890625)^(−1/4))
 *
 * where S is the current production store content and X1 is its capacity.
 * The output is routed to the uh_input brick.
 */
class ProcessPercolationGR4J : public ProcessOutflow {
  public:
    explicit ProcessPercolationGR4J(WaterContainer* container);

    ~ProcessPercolationGR4J() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessParametersAndForcing(SettingsModel* modelSettings);

  protected:
    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_PERCOLATION_GR4J_H
