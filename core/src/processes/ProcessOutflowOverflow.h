#ifndef HYDROBRICKS_PROCESS_OVERFLOW_H
#define HYDROBRICKS_PROCESS_OVERFLOW_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

class ProcessOutflowOverflow : public ProcessOutflow {
  public:
    explicit ProcessOutflowOverflow(WaterContainer* container);

    ~ProcessOutflowOverflow() override = default;

    /**
     * @copydoc Process::AssignParameters()
     */
    void AssignParameters(const ProcessSettings& processSettings) override;

    void StoreInOutgoingFlux(double* rate, int index) override;

  protected:
    vecDouble GetRates() override;

  private:
};

#endif  // HYDROBRICKS_PROCESS_OVERFLOW_H
