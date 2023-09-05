#ifndef HYDROBRICKS_PROCESS_RUNOFF_SOCONT_H
#define HYDROBRICKS_PROCESS_RUNOFF_SOCONT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

class ProcessRunoffSocont : public ProcessOutflow {
  public:
    explicit ProcessRunoffSocont(WaterContainer* container);

    ~ProcessRunoffSocont() override = default;

    /**
     * @copydoc Process::AssignParameters()
     */
    void AssignParameters(const ProcessSettings& processSettings) override;

  protected:
    float* m_slope; // [ratio]
    float* m_beta;  // []

    vecDouble GetRates() override;

  private:
};

#endif  // HYDROBRICKS_PROCESS_RUNOFF_SOCONT_H
