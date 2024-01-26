#ifndef HYDROBRICKS_PROCESS_OUTFLOW_PERCOLATION_H
#define HYDROBRICKS_PROCESS_OUTFLOW_PERCOLATION_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

class ProcessOutflowPercolation : public ProcessOutflow {
  public:
    explicit ProcessOutflowPercolation(WaterContainer* container);

    ~ProcessOutflowPercolation() override = default;

    /**
     * @copydoc Process::SetParameters()
     */
    void SetParameters(const ProcessSettings& processSettings) override;

  protected:
    float* m_rate;  // [mm/d]

    vecDouble GetRates() override;

  private:
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_CONSTANT_H
