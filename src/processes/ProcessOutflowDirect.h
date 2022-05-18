#ifndef HYDROBRICKS_PROCESS_OUTFLOW_DIRECT_H
#define HYDROBRICKS_PROCESS_OUTFLOW_DIRECT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

class ProcessOutflowDirect : public ProcessOutflow {
  public:
    explicit ProcessOutflowDirect(Brick* brick);

    ~ProcessOutflowDirect() override = default;

    vecDouble GetChangeRates() override;

  protected:

  private:
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_DIRECT_H
