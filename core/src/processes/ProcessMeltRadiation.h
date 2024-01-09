#ifndef HYDROBRICKS_PROCESS_MELT_RADIATION_H
#define HYDROBRICKS_PROCESS_MELT_RADIATION_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessMelt.h"

class ProcessMeltRadiation : public ProcessMelt {
  public:
    explicit ProcessMeltRadiation(WaterContainer* container);

    ~ProcessMeltRadiation() override = default;

    /**
     * @copydoc Process::IsOk()
     */
    bool IsOk() override;

    void SetParameters(const ProcessSettings& processSettings) override;

    void AttachForcing(Forcing* forcing) override;

  protected:
    Forcing* m_temperature;
    Forcing* m_potentialClearSkyDirectSolarRadiation;
    float* m_meltingTemperature;
    float* m_radiationCoefficient;

    vecDouble GetRates() override;

  private:
};

#endif  // HYDROBRICKS_PROCESS_MELT_RADIATION_H
