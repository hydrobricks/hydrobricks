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
    void AssignParameters(const ProcessSettings &processSettings) override;

    vecDouble GetChangeRates() override;

  protected:
    float* m_slope;  // [ratio]
    float* m_runoffParameter;  // []
    double m_h;  // [m]

  private:
};

#endif  // HYDROBRICKS_PROCESS_RUNOFF_SOCONT_H
