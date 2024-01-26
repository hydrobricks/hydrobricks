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
     * @copydoc Process::IsOk()
     */
    bool IsOk() override;

    void SetHydroUnitProperties(HydroUnit* unit, Brick* brick) override;

    void SetParameters(const ProcessSettings& processSettings) override;

    void AttachForcing(Forcing* forcing) override;

  protected:
    string m_aspect_class;
    Forcing* m_temperature;
    float* m_degreeDayFactor;
    float* m_meltingTemperature;

    vecDouble GetRates() override;

  private:
};

#endif  // HYDROBRICKS_PROCESS_MELT_DEGREE_DAY_ASPECT_H
