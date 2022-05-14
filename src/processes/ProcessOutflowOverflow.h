#ifndef HYDROBRICKS_PROCESS_OVERFLOW_H
#define HYDROBRICKS_PROCESS_OVERFLOW_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

class ProcessOutflowOverflow : public ProcessOutflow {
  public:
    explicit ProcessOutflowOverflow(Brick* brick);

    ~ProcessOutflowOverflow() override = default;

    /**
     * @copydoc Process::AssignParameters()
     */
    void AssignParameters(const ProcessSettings &processSettings) override;

    vecDouble GetChangeRates() override;

    void StoreInOutgoingFlux(double* rate, int index) override;

  protected:

  private:
};

#endif  // HYDROBRICKS_PROCESS_OVERFLOW_H
