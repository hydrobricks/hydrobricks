#ifndef HYDROBRICKS_PROCESS_MELT_CEMANEIGE_H
#define HYDROBRICKS_PROCESS_MELT_CEMANEIGE_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessMelt.h"

/**
 * CemaNeige snowmelt process (Valéry et al., 2014).
 *
 * Extends a simple degree-day approach with a cold content (thermal state)
 * correction that delays melt after cold periods (Valéry et al., 2014):
 *
 *   eTG ← min(0, CTG × eTG_prev + (1 − CTG) × (T − Tmelt))
 *   PotMelt = Kf × (T − Tmelt)  if eTG == 0 and T > Tmelt, else 0
 *   Melt = (0.9 × min(1, SWE / Cn) + 0.1) × PotMelt
 *
 * Parameters:
 *   degree_day_factor (Kf)       [mm/°C/d]
 *   cold_content_factor (CTG)    [-]  weighting for cold content update
 *   melting_temperature (Tmelt)  [°C]
 *   mean_annual_snow              [mm] mean annual solid precipitation; Cn = 0.9 * this value
 *
 * Internal state _coldContent (G) persists between timesteps and is reset
 * to zero by Reset().
 */
class ProcessMeltCemaNeige : public ProcessMelt {
  public:
    explicit ProcessMeltCemaNeige(WaterContainer* container);

    ~ProcessMeltCemaNeige() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessParametersAndForcing(SettingsModel* modelSettings);

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

    /**
     * @copydoc Process::Reset()
     */
    void Reset() override;

  protected:
    Forcing* _temperature;            // non-owning reference
    const float* _degreeDayFactor;    // Kf [mm/°C/d]
    const float* _coldContentFactor;  // CTG [-]
    const float* _meltingTemp;        // Tmelt [°C]
    const float* _meanAnnualSnow;     // Cn [mm]

    double _coldContent;  // G [°C], persists between timesteps

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_MELT_CEMANEIGE_H
