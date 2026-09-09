#ifndef HYDROBRICKS_PROCESS_OUTFLOW_SNOW_HOLDING_PREVAH_H
#define HYDROBRICKS_PROCESS_OUTFLOW_SNOW_HOLDING_PREVAH_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflowSnowHolding.h"

/**
 * PREVAH snowpack liquid water release (mxp_snow.f90 s_snowmelt).
 *
 * As 'outflow:snow_holding', but with PREVAH's temperature-dependent retention:
 * - On cold days (T <= melting_temperature) the liquid water (rain on snow and melt
 *   water) is held up to whc x SWE, like the HBV holding.
 * - On melt days (T > melting_temperature) PREVAH's ablation branch computes the
 *   retention limit from the LIQUID content itself (sliqmax = CWH * snow_liquid,
 *   mxp_snow.f90), so the store drains to whc x liquid — about (1 - whc) of the
 *   liquid water leaves every melt day. This releases the retained water much
 *   faster than the HBV holding and is reproduced literally here.
 *
 * With a positive liquid_release_exponent (CEXLIQ) the melt-day release additionally
 * reproduces PREVAH's graded partition of the NEW melt M: a fraction atsliq of the
 * fresh melt passes straight through, the rest joins the liquid store before the
 * (1 - whc) collapse. atsliq = clamp((L / (whc * solid))^CEXLIQ, 0, 1) where L is the
 * liquid before the melt and solid the snow after it. The whole ablation-day release
 * then collapses to
 *   release = (1 - whc) * C + whc * atsliq * M,   C = L + M (current liquid content),
 * which is exactly the sequence of mxp_snow.f90 lines 342-361. With CEXLIQ = 0 (the
 * default) atsliq = 0 and the release is the plain (1 - whc) * C collapse.
 */

class ProcessMelt;
class ProcessOutflowSnowHoldingPrevah : public ProcessOutflowSnowHolding {
  public:
    explicit ProcessOutflowSnowHoldingPrevah(WaterContainer* container);

    ~ProcessOutflowSnowHoldingPrevah() override = default;

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
     * @copydoc Process::AttachForcing()
     */
    void AttachForcing(Forcing* forcing) override;

  protected:
    Forcing* _temperature;                         // non-owning reference
    const float* _meltingTemperature;              // T0, the PREVAH melt threshold [deg C]
    const float* _liquidReleaseExponent{nullptr};  // CEXLIQ, the graded-release exponent [-]
    ProcessMelt* _meltProcess{nullptr};            // sibling melt process (cached), for M

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;

    /**
     * Find the melt process on the same snowpack brick (any melt kind).
     *
     * @return pointer to the sibling melt process, or nullptr if not found.
     */
    [[nodiscard]] ProcessMelt* FindSiblingMeltProcess() const;

    /**
     * Get the melt water delivered to the snowpack liquid store this time step.
     *
     * @return the melt amount M [mm], read from the sibling melt process' instantaneous
     * output flux (0 if there is no melt process or no melt this step).
     */
    [[nodiscard]] double GetMeltThisStep() const;
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_SNOW_HOLDING_PREVAH_H
