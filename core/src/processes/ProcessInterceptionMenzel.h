#ifndef HYDROBRICKS_PROCESS_INTERCEPTION_MENZEL_H
#define HYDROBRICKS_PROCESS_INTERCEPTION_MENZEL_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * Canopy throughfall with Menzel (1997) asymptotic filling (PREVAH interception).
 *
 * The canopy intercepts only a diminishing fraction of the incoming rain as it fills,
 * so part of the rain passes through even before the storage is full:
 *
 *   Δs = (si_max − S) × (1 − exp(−rain / si_max))    (retained this step)
 *   throughfall = rain − Δs   (+ any excess above si_max)
 *
 * si_max is the interception capacity [mm] and S the storage before the rain. This
 * contrasts with outflow:threshold, which retains all rain until the capacity is full
 * and only then spills. Meant as the throughfall process of a canopy (interception
 * storage) brick on the direct (pre-solver) pass, so the incoming rain is available via
 * the container's incoming fluxes and the retained water then evaporates at the
 * potential rate (et:open_water).
 */
class ProcessInterceptionMenzel : public ProcessOutflow {
  public:
    explicit ProcessInterceptionMenzel(WaterContainer* container);

    ~ProcessInterceptionMenzel() override = default;

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
    const float* _capacity;  // interception capacity si_max [mm]

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_INTERCEPTION_MENZEL_H
