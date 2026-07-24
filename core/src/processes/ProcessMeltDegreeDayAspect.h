#ifndef HYDROBRICKS_PROCESS_MELT_DEGREE_DAY_ASPECT_H
#define HYDROBRICKS_PROCESS_MELT_DEGREE_DAY_ASPECT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessMelt.h"

/**
 * Degree-day melt with an aspect-dependent factor.
 *
 * Same degree-day formulation as melt:degree_day, but the degree-day factor is
 * selected from the hydro unit aspect class (north / south / east-west):
 *   M = cfmax(aspect) × (T − TT)    for T ≥ TT, else 0
 *
 * Typically lower on shaded (north) slopes and higher on sunny (south) slopes.
 * Requires temperature forcing and an 'aspect_class' hydro unit property.
 */
class ProcessMeltDegreeDayAspect : public ProcessMelt {
  public:
    explicit ProcessMeltDegreeDayAspect(WaterContainer* container);

    ~ProcessMeltDegreeDayAspect() override = default;

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
     * @copydoc Process::SetHydroUnitProperties()
     */
    void SetHydroUnitProperties(HydroUnit* unit, Brick* brick) override;

    /**
     * @copydoc Process::SetParameters()
     */
    void SetParameters(const ProcessSettings& processSettings) override;

    /**
     * @copydoc Process::AttachForcing()
     */
    void AttachForcing(Forcing* forcing) override;

  protected:
    string _aspectClass;
    Forcing* _temperature;  // non-owning reference
    const float* _degreeDayFactor;
    const float* _meltingTemperature;

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_MELT_DEGREE_DAY_ASPECT_H
