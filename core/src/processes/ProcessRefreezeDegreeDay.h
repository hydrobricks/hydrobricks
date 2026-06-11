#ifndef HYDROBRICKS_PROCESS_REFREEZE_DEGREE_DAY_H
#define HYDROBRICKS_PROCESS_REFREEZE_DEGREE_DAY_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessTransform.h"

class ProcessMeltDegreeDay;

/**
 * Degree-day refreezing of the snowpack liquid water (HBV; Lindström et al., 1997).
 *
 * Lives on the water container of a snowpack brick (requires snowpacks
 * generated with water retention) and refreezes the retained liquid water back
 * to snow when the air temperature is below the melting temperature:
 *   RF = cfr × cfmax × (TT − T)    for T < TT
 *
 * The degree-day factor (cfmax) and the melting temperature (TT) are read from
 * the melt:degree_day process of the same brick, so that melt and refreeze stay
 * consistent; only the dimensionless refreezing coefficient cfr is a parameter
 * of this process. The output flux must target the same brick with the snow
 * content type (e.g. 'ground_snowpack:snow').
 */
class ProcessRefreezeDegreeDay : public ProcessTransform {
  public:
    explicit ProcessRefreezeDegreeDay(WaterContainer* container);

    ~ProcessRefreezeDegreeDay() override = default;

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
    Forcing* _temperature;           // non-owning reference
    const float* _refreezingFactor;  // cfr [-]

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;

  private:
    ProcessMeltDegreeDay* _meltProcess;  // non-owning reference (sibling process, cached)

    /**
     * Find the melt:degree_day process on the same brick.
     *
     * @return pointer to the melt process, or nullptr if not found.
     */
    [[nodiscard]] ProcessMeltDegreeDay* FindSiblingMeltProcess() const;
};

#endif  // HYDROBRICKS_PROCESS_REFREEZE_DEGREE_DAY_H
