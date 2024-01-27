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
     * @copydoc Process::IsOk()
     */
    bool IsOk() override;

    void SetParameters(const ProcessSettings& processSettings) override;

    void AttachForcing(Forcing* forcing) override;

  protected:
    Forcing* m_temperature;
    Forcing* m_potentialClearSkyDirectSolarRadiation;
    float* m_meltFactor;
    float* m_meltingTemperature;
    float* m_radiationCoefficient;

    vecDouble GetRates() override;

  private:
};

#endif  // HYDROBRICKS_PROCESS_MELT_TEMPERATURE_INDEX_H
