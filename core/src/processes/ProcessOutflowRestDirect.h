#ifndef HYDROBRICKS_PROCESS_OUTFLOW_REST_DIRECT_H
#define HYDROBRICKS_PROCESS_OUTFLOW_REST_DIRECT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

class ProcessOutflowRestDirect : public ProcessOutflow {
  public:
    explicit ProcessOutflowRestDirect(WaterContainer* container);

    ~ProcessOutflowRestDirect() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessParametersAndForcing(SettingsModel* modelSettings);

  protected:
    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_REST_DIRECT_H
