#ifndef HYDROBRICKS_PROCESS_OUTFLOW_CONSTANT_H
#define HYDROBRICKS_PROCESS_OUTFLOW_CONSTANT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

class ProcessOutflowConstant : public ProcessOutflow {
  public:
    explicit ProcessOutflowConstant(WaterContainer* container);

    ~ProcessOutflowConstant() override = default;

    /**
     * @copydoc Process::AssignParameters()
     */
    void AssignParameters(const ProcessSettings &processSettings) override;

  protected:
    float* m_rate;  // [mm/d]

    vecDouble GetRates() override;

  private:
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_CONSTANT_H
