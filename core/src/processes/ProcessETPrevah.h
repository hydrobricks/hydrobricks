#ifndef HYDROBRICKS_PROCESS_ET_PREVAH_H
#define HYDROBRICKS_PROCESS_ET_PREVAH_H

#include "Includes.h"
#include "ProcessETHBV.h"

class LandCover;

/**
 * PREVAH actual evapotranspiration: the HBV soil-moisture limitation with the
 * PREVAH snow-albedo reduction of the potential rate.
 *
 * PREVAH's PET carries a (1 - albedo)/0.8 term: over snow (albedo ~0.7-0.8) the
 * potential evaporation drops to ~0.25-0.4x, which also acts as the ET-under-snow
 * suppression. Here the albedo is derived at runtime rom the hydro unit's snowpacks:
 * the snow-covered fraction is the sum of the parent land-cover fractions of the
 * snowpacks currently holding snow, and the albedo interpolates between the
 * snow-free and snow albedo accordingly:
 *   snowfrac = sum_c fraction_c * (SWE_c > 0)
 *   albedo   = albedo_land + snowfrac * (albedo_snow - albedo_land)
 *   Ea       = cevpf * PET * (1 - albedo)/0.8 * min(SM / (LP * FC), 1)
 *
 * PREVAH's s_eta_soil has two branches, and 'potential_fraction' is the share of the
 * unit taking the first one:
 *   - the potential branch (fraction 1), which evaporates at the full potential rate
 *     whatever the soil moisture. PREVAH takes it on the vegetated covers when its
 *     et_calc_method is 2 or 3;
 *   - the soil-moisture branch (fraction 0, the default), which applies the LP * FC
 *     limitation above, and evaporates *nothing* where that threshold is zero
 *     ('IF (sgrlp .le. 0.0) THEN EA = 0.0'): bare rock on no soil has no water to
 *     give. PREVAH always takes it on its water, built-up and rock covers.
 * The two are mixed linearly, so a value in between stands for a unit split between
 * the branches.
 *
 * With no snow (and the default albedo_land = 0.2) the factor is exactly 1, so the
 * process then behaves identically to 'et:hbv'. Requires a hydro-unit context (the
 * process must live on a hydro-unit brick, e.g. the soil moisture store).
 *
 * A land cover can be given as gate brick, for a soil store that belongs to a single
 * cover: the albedo is then read from the snow of that cover alone, as a PREVAH
 * hydrotope does, instead of the mean over the unit.
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

    /**
     * @copydoc Process::AddGateBrick()
     *
     * The gate brick is optional here; when given, it must be the land cover owning
     * the soil store, and the albedo is read from the snow of that cover alone.
     */
    void AddGateBrick(Brick* brick) override;

  protected:
    const float* _albedoLand;         // snow-free ground albedo [-] (0.2 makes the factor neutral)
    const float* _potentialFraction;  // share evaporating at the potential rate [-]
    const LandCover* _cover;          // cover owning the store, if any (nullptr: the whole unit)

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_ET_PREVAH_H
