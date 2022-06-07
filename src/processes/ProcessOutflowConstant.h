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

    vecDouble GetChangeRates() override;

  protected:
    float* m_rate;  // [mm/d]

  private:
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_CONSTANT_H
