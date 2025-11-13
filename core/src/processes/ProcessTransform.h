#ifndef HYDROBRICKS_PROCESS_TRANSFORM_H
#define HYDROBRICKS_PROCESS_TRANSFORM_H

#include "Forcing.h"
#include "Includes.h"
#include "Process.h"

class ProcessTransform : public Process {
  public:
    explicit ProcessTransform(WaterContainer* container);

    ~ProcessTransform() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessParametersAndForcing(SettingsModel* modelSettings);

    /**
     * @copydoc Process::IsOk()
     */
    [[nodiscard]] bool IsOk() override;

    /**
     * @copydoc Process::GetConnectionsNb()
     */
    int GetConnectionsNb() override;

    /**
     * @copydoc Process::GetValuePointer()
     */
    double* GetValuePointer(const string& name) override;
};

#endif  // HYDROBRICKS_PROCESS_TRANSFORM_H
