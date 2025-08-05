#ifndef HYDROBRICKS_PROCESS_OUTFLOW_H
#define HYDROBRICKS_PROCESS_OUTFLOW_H

#include "Forcing.h"
#include "Includes.h"
#include "Process.h"

class ProcessOutflow : public Process {
  public:
    explicit ProcessOutflow(WaterContainer* container);

    ~ProcessOutflow() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessParametersAndForcing(SettingsModel* modelSettings);

    /**
     * @copydoc Process::IsOk()
     */
    bool IsOk() override;

    /**
     * @copydoc Process::GetConnectionsNb()
     */
    int GetConnectionsNb() override;

    /**
     * @copydoc Process::GetValuePointer()
     */
    double* GetValuePointer(const string& name) override;
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_H
