#ifndef HYDROBRICKS_PROCESS_OUTFLOW_DIRECT_H
#define HYDROBRICKS_PROCESS_OUTFLOW_DIRECT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

class ProcessOutflowDirect : public ProcessOutflow {
  public:
    explicit ProcessOutflowDirect(WaterContainer* container);

    ~ProcessOutflowDirect() override = default;

    static void RegisterProcessParametersAndForcing(SettingsModel* modelSettings);

  protected:
    vecDouble GetRates() override;

  private:
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_DIRECT_H
