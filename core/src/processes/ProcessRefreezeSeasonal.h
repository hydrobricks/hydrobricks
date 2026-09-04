#ifndef HYDROBRICKS_PROCESS_REFREEZE_SEASONAL_H
#define HYDROBRICKS_PROCESS_REFREEZE_SEASONAL_H

#include "Includes.h"
#include "ProcessRefreezeDegreeDay.h"

/**
 * Seasonal degree-day refreezing of the snowpack liquid water (PREVAH).
 *
 * As 'refreeze:degree_day', but with its own seasonal degree-day factor instead of
 * reading it from a melt:degree_day sibling — PREVAH refreezes with the seasonal PDDI
 * factor (CRMFMIN/CRMFMAX) on cold days even when the melt itself uses the Hock
 * radiation equation (mxp_snow.f90: wstar = f_melting_eq(2, season_mf) * CRFR):
 *   RF = cfr x (mean + amp x sin(2 pi (doy - 80)/366)) x (T0 - T)   for T < T0
 *
 * The seasonal sine is identical to PREVAH's f_season_fact. Use it whenever the melt
 * process is not a degree-day one (e.g. 'melt:temperature_index').
 */
class ProcessRefreezeSeasonal : public ProcessRefreezeDegreeDay {
  public:
    explicit ProcessRefreezeSeasonal(WaterContainer* container);

    ~ProcessRefreezeSeasonal() override = default;

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
    const float* _degreeDayFactorMin;  // seasonal factor minimum (winter) [mm/d/degC]
    const float* _degreeDayFactorMax;  // seasonal factor maximum (summer) [mm/d/degC]
    const float* _meltingTemperature;  // T0 [degC]

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_REFREEZE_SEASONAL_H
