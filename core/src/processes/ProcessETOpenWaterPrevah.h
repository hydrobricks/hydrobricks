#ifndef HYDROBRICKS_PROCESS_ET_OPEN_WATER_PREVAH_H
#define HYDROBRICKS_PROCESS_ET_OPEN_WATER_PREVAH_H

#include "Includes.h"
#include "ProcessETOpenWater.h"

/**
 * Open-water (potential) evaporation with the PREVAH snow-albedo reduction.
 *
 * As 'et:open_water' (evaporates at the potential rate, capped at the available
 * content), but the potential rate carries PREVAH's (1 - albedo)/0.8 term, with the
 * albedo interpolated between the snow-free and snow albedo by the hydro unit's
 * snow-covered fraction (see HydroUnit::GetSnowCoverFraction):
 *   Ea = min(et_factor * PET * (1 - albedo)/0.8, content / dt)
 *
 * Used for the canopy evaporation of an interception store in PREVAH setups (the
 * interception ET draws from the albedo-reduced PET, scaled by the vegetation-cover
 * fraction veg_cov via the et_factor parameter, which can be made monthly). With no
 * snow (and the defaults albedo_land = 0.2, et_factor = 1) the rate matches
 * 'et:open_water' exactly. Requires a hydro-unit context (hydro-unit bricks only).
 */
class ProcessETOpenWaterPrevah : public ProcessETOpenWater {
  public:
    explicit ProcessETOpenWaterPrevah(WaterContainer* container);

    ~ProcessETOpenWaterPrevah() override = default;

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
    const float* _albedoLand;         // snow-free ground albedo [-] (0.2 makes the factor neutral)
    const float* _etFactor{nullptr};  // scaling of the potential rate [-] (PREVAH veg_cov)

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_ET_OPEN_WATER_PREVAH_H
