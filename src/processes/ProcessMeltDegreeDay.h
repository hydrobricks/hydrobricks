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
     * @copydoc Process::IsOk()
     */
    bool IsOk() override;

    void AssignParameters(const ProcessSettings &processSettings) override;

    void AttachForcing(Forcing* forcing) override;

    vecDouble GetChangeRates() override;

  protected:
    Forcing* m_temperature;
    float* m_degreeDayFactor;
    float* m_meltingTemperature;

  private:
};

#endif  // HYDROBRICKS_PROCESS_MELT_DEGREE_DAY_H
