#ifndef HYDROBRICKS_PROCESS_SUBLIMATION_PREVAH_H
#define HYDROBRICKS_PROCESS_SUBLIMATION_PREVAH_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessSublimation.h"

class Snowpack;

/**
 * PREVAH snow evaporation.
 *
 * PREVAH evaporates snow at the (albedo-reduced) potential rate whenever snow is
 * present (s_snoevap1.f90, default: et_snow = et_pot):
 *   S = PET * (1 - albedo)/0.8
 * where the albedo is the snowpack's own age-dependent snow albedo
 * (Snowpack::GetSnowAlbedo, 0.4-0.85). Old (spring) snow has a low albedo, so it
 * evaporates strongly - the dominant snow-evaporation sink. Requires PET forcing and a
 * hydro-unit snowpack context; no evaporation when the snow container is empty.
 */
class ProcessSublimationPrevah : public ProcessSublimation {
  public:
    explicit ProcessSublimationPrevah(WaterContainer* container);

    ~ProcessSublimationPrevah() override = default;

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

    /**
     * @copydoc Process::HasConstraintPriority()
     *
     * PREVAH serves snow evaporation sequentially BEFORE melt (s_SNOEVAP1 is called ahead
     * of s_snowmelt in sxp_core.f08), so it gets the full potential rate even on days the
     * pack drains completely; melt only takes what remains.
     */
    [[nodiscard]] bool HasConstraintPriority() const override {
        return true;
    }

  protected:
    Forcing* _pet;        // non-owning reference
    Snowpack* _snowpack;  // non-owning reference (the owning snowpack), resolved lazily

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_SUBLIMATION_PREVAH_H
