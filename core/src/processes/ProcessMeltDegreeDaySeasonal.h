#ifndef HYDROBRICKS_PROCESS_MELT_DEGREE_DAY_SEASONAL_H
#define HYDROBRICKS_PROCESS_MELT_DEGREE_DAY_SEASONAL_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessMeltDegreeDay.h"

/**
 * Degree-day snow/ice melt with a seasonally varying melt factor (PREVAH).
 *
 * The degree-day factor follows a sine over the year between a winter minimum
 * and a summer maximum (PREVAH's CRMFMIN/CRMFMAX; Zappa et al., 2003):
 *
 *   DDF(doy) = (max + min)/2 + (max − min)/2 × sin(2π (doy − 80) / 366)
 *   M = DDF(doy) × (T − TT)    for T ≥ TT, else 0
 *
 * The phase matches the PREVAH formulation: zero crossing at the spring equinox
 * (doy 80), maximum at the summer solstice, minimum at the winter solstice.
 * (PREVAH shifts the phase by one day in leap years; this is neglected here.)
 * Requires temperature forcing; without a time machine (unit tests) the mean
 * factor is used.
 *
 * Subclasses ProcessMeltDegreeDay so companion processes reading the current
 * degree-day factor (e.g. refreeze:degree_day) transparently get the seasonal
 * value, as in PREVAH (refreezing = CRFR × RMF(doy) × (TT − T)).
 */
class ProcessMeltDegreeDaySeasonal : public ProcessMeltDegreeDay {
  public:
    explicit ProcessMeltDegreeDaySeasonal(WaterContainer* container);

    ~ProcessMeltDegreeDaySeasonal() override = default;

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
     * Get the seasonal degree-day factor for the current simulation day.
     *
     * @return The degree-day factor [mm/d/°C].
     */
    [[nodiscard]] double GetDegreeDayFactor() const override;

  protected:
    const float* _degreeDayFactorMin;
    const float* _degreeDayFactorMax;

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_MELT_DEGREE_DAY_SEASONAL_H
