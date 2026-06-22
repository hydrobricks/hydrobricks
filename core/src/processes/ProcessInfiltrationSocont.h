#ifndef HYDROBRICKS_PROCESS_INFILTRATION_SOCONT_H
#define HYDROBRICKS_PROCESS_INFILTRATION_SOCONT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessInfiltration.h"

/**
 * Socont infiltration into the soil storage.
 *
 * Splits the incoming water according to the filling ratio of the target soil
 * storage (capacity S_max):
 *   infiltration = in × (1 − (S/S_max)^2)
 *
 * The complement, in × (S/S_max)^2, is the surface runoff and must be routed by a
 * subsequent outflow:rest process. With no target capacity the infiltration is zero.
 */
class ProcessInfiltrationSocont : public ProcessInfiltration {
  public:
    explicit ProcessInfiltrationSocont(WaterContainer* container);

    ~ProcessInfiltrationSocont() override = default;

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
    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_INFILTRATION_SOCONT_H
