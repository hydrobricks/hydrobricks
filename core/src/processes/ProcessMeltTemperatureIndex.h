#ifndef HYDROBRICKS_PROCESS_MELT_TEMPERATURE_INDEX_H
#define HYDROBRICKS_PROCESS_MELT_TEMPERATURE_INDEX_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessMelt.h"

class ProcessMeltTemperatureIndex : public ProcessMelt {
  public:
    explicit ProcessMeltTemperatureIndex(WaterContainer* container);

    ~ProcessMeltTemperatureIndex() override = default;

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
    Forcing* _temperature;
    Forcing* _potentialClearSkyDirectSolarRadiation;
    float* _meltFactor;
    float* _meltingTemperature;
    float* _radiationCoefficient;

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_MELT_TEMPERATURE_INDEX_H
