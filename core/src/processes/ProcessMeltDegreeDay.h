#ifndef HYDROBRICKS_PROCESS_MELT_DEGREE_DAY_H
#define HYDROBRICKS_PROCESS_MELT_DEGREE_DAY_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessMelt.h"

class ProcessMeltDegreeDay : public ProcessMelt {
  public:
    explicit ProcessMeltDegreeDay(WaterContainer* container);

    ~ProcessMeltDegreeDay() override = default;

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
     * @copydoc Process::SetParameters()
     */
    void SetParameters(const ProcessSettings& processSettings) override;

    /**
     * @copydoc Process::AttachForcing()
     */
    void AttachForcing(Forcing* forcing) override;

  protected:
    Forcing* m_temperature;
    float* m_degreeDayFactor;
    float* m_meltingTemperature;

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_MELT_DEGREE_DAY_H
