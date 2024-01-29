#ifndef HYDROBRICKS_PROCESS_OUTFLOW_REST_DIRECT_H
#define HYDROBRICKS_PROCESS_OUTFLOW_REST_DIRECT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

class ProcessOutflowRestDirect : public ProcessOutflow {
  public:
    explicit ProcessOutflowRestDirect(WaterContainer* container);

    ~ProcessOutflowRestDirect() override = default;

    static void RegisterProcessParametersAndForcing(SettingsModel* modelSettings);

  protected:
    vecDouble GetRates() override;

  private:
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_REST_DIRECT_H
