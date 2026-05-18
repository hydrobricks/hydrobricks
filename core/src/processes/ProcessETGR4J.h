#ifndef HYDROBRICKS_PROCESS_ET_GR4J_H
#define HYDROBRICKS_PROCESS_ET_GR4J_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessET.h"

/**
 * GR4J actual evapotranspiration from the production store (Es).
 *
 * Active only when En > 0 (deficit conditions), where En = max(0, E − P_total) is published
 * to the PET forcing by ProcessInterceptionGR4J before this process runs:
 *   Es = S × (2 − S/X1) × tanh(En/X1) / (1 + (1 − S/X1) × tanh(En/X1))
 *
 * Requires only PET forcing (which carries En after the interception step).
 */
class ProcessETGR4J : public ProcessET {
  public:
    explicit ProcessETGR4J(WaterContainer* container);

    ~ProcessETGR4J() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessParametersAndForcing(SettingsModel* modelSettings);

    /**
     * @copydoc Process::IsValid()
     */
    [[nodiscard]] bool IsValid() const override;

    /**
     * @copydoc Process::AttachForcing()
     */
    void AttachForcing(Forcing* forcing) override;

  protected:
    Forcing* _pet;  // non-owning reference; holds En after interception updates it

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_ET_GR4J_H
