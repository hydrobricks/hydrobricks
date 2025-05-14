#ifndef HYDROBRICKS_PROCESS_MELT_DEGREE_DAY_ASPECT_H
#define HYDROBRICKS_PROCESS_MELT_DEGREE_DAY_ASPECT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessMelt.h"

class ProcessMeltDegreeDayAspect : public ProcessMelt {
  public:
    explicit ProcessMeltDegreeDayAspect(WaterContainer* container);

    ~ProcessMeltDegreeDayAspect() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessParametersAndForcing(SettingsModel* modelSettings);

    /**
     * @copydoc Process::IsOk()
     */
    bool IsOk() override;

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
    string m_aspectClass;
    Forcing* m_temperature;
    float* m_degreeDayFactor;
    float* m_meltingTemperature;

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_MELT_DEGREE_DAY_ASPECT_H
