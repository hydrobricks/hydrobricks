#ifndef HYDROBRICKS_PROCESS_OUTFLOW_DIRECT_H
#define HYDROBRICKS_PROCESS_OUTFLOW_DIRECT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

class ProcessOutflowDirect : public ProcessOutflow {
  public:
    explicit ProcessOutflowDirect(WaterContainer* container);

    ~ProcessOutflowDirect() override = default;

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

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_DIRECT_H
