#ifndef HYDROBRICKS_PROCESS_RUNOFF_SOCONT_H
#define HYDROBRICKS_PROCESS_RUNOFF_SOCONT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

class HydroUnit;

class ProcessRunoffSocont : public ProcessOutflow {
  public:
    explicit ProcessRunoffSocont(WaterContainer* container);

    ~ProcessRunoffSocont() override = default;

    /**
     * @copydoc Process::SetHydroUnitProperties()
     */
    void SetHydroUnitProperties(HydroUnit* unit, Brick* brick) override;

    /**
     * @copydoc Process::SetParameters()
     */
    void SetParameters(const ProcessSettings& processSettings) override;

  protected:
    float m_slope;           // []
    float* m_beta;           // []
    double* m_areaFraction;  // []
    double m_areaUnit;       // [m^2]

    vecDouble GetRates() override;

    double GetArea();

  private:
};

#endif  // HYDROBRICKS_PROCESS_RUNOFF_SOCONT_H
