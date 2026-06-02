#ifndef HYDROBRICKS_PROCESS_INTERCEPTION_GR4J_H
#define HYDROBRICKS_PROCESS_INTERCEPTION_GR4J_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessET.h"

/**
 * GR4J P−E neutralization on the ground land cover.
 *
 * Sends min(P, E) to atmosphere each timestep so that the downstream
 * infiltration and rest_direct processes see only net precipitation Pn.
 * When P > E, sends E. When P ≤ E, sends all of P (production store then
 * handles the remaining deficit En via et:gr4j).
 */
class ProcessInterceptionGR4J : public ProcessET {
  public:
    explicit ProcessInterceptionGR4J(WaterContainer* container);

    ~ProcessInterceptionGR4J() override = default;

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
     * @copydoc Process::AttachForcing()
     */
    void AttachForcing(Forcing* forcing) override;

  protected:
    Forcing* _pet;  // non-owning reference

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_INTERCEPTION_GR4J_H
