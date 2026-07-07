#ifndef HYDROBRICKS_PROCESS_INFILTRATION_GR4J_H
#define HYDROBRICKS_PROCESS_INFILTRATION_GR4J_H

#include "Includes.h"
#include "ProcessInfiltration.h"

/**
 * GR4J production-store infiltration (Ps).
 *
 * Reads net precipitation Pn directly from the container (ground_soil brick,
 * which receives only Pn via the instantaneous throughfall from the ground
 * land-cover brick) and computes the portion entering the production store:
 *   Ps = X1 × (1 − (S/X1)²) × tanh(Pn/X1) / (1 + (S/X1) × tanh(Pn/X1))
 *
 * Requires a linked target brick (production_store) to read its current
 * filling ratio and capacity (X1). No forcing needed.
 */
class ProcessInfiltrationGR4J : public ProcessInfiltration {
  public:
    explicit ProcessInfiltrationGR4J(WaterContainer* container);

    ~ProcessInfiltrationGR4J() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessSettings(SettingsModel* modelSettings);

  protected:
    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_INFILTRATION_GR4J_H
