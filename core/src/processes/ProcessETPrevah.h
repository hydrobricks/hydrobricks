#ifndef HYDROBRICKS_PROCESS_ET_PREVAH_H
#define HYDROBRICKS_PROCESS_ET_PREVAH_H

#include "Includes.h"
#include "ProcessETHBV.h"

/**
 * PREVAH actual evapotranspiration: the HBV soil-moisture limitation with the
 * PREVAH snow-albedo reduction of the potential rate.
 *
 * PREVAH's Hamon PET carries a (1 - albedo)/0.8 term (mxp_evapotranspiration.f08):
 * over snow (albedo ~0.7-0.8) the potential evaporation drops to ~0.25-0.4x, which
 * also acts as the ET-under-snow suppression. Here the albedo is derived at runtime
 * from the hydro unit's snowpacks: the snow-covered fraction is the sum of the
 * parent land-cover fractions of the snowpacks currently holding snow, and the
 * albedo interpolates between the snow-free and snow albedo accordingly:
 *   snowfrac = sum_c fraction_c * (SWE_c > 0)
 *   albedo   = albedo_land + snowfrac * (albedo_snow - albedo_land)
 *   Ea       = cevpf * PET * (1 - albedo)/0.8 * min(SM / (LP * FC), 1)
 *
 * With no snow (and the default albedo_land = 0.2) the factor is exactly 1, so the
 * process then behaves identically to 'et:hbv'. Requires a hydro-unit context (the
 * process must live on a hydro-unit brick, e.g. the soil moisture store).
 */
class ProcessETPrevah : public ProcessETHBV {
  public:
    explicit ProcessETPrevah(WaterContainer* container);

    ~ProcessETPrevah() override = default;

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
    const float* _albedoLand;  // snow-free ground albedo [-] (0.2 makes the factor neutral)

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_ET_PREVAH_H
