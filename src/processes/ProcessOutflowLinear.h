#ifndef HYDROBRICKS_PROCESS_OUTFLOW_LINEAR_H
#define HYDROBRICKS_PROCESS_OUTFLOW_LINEAR_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

class ProcessOutflowLinear : public ProcessOutflow {
  public:
    explicit ProcessOutflowLinear(Brick* brick);

    ~ProcessOutflowLinear() override = default;

    /**
     * @copydoc Process::AssignParameters()
     */
    void AssignParameters(const ProcessSettings &processSettings) override;

    vecDouble GetChangeRates() override;

  protected:
    float* m_responseFactor;  // [1/d]

  private:
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_LINEAR_H
