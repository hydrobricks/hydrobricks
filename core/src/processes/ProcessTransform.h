#ifndef HYDROBRICKS_PROCESS_TRANSFORM_H
#define HYDROBRICKS_PROCESS_TRANSFORM_H

#include "Forcing.h"
#include "Includes.h"
#include "Process.h"

class ProcessTransform : public Process {
  public:
    explicit ProcessTransform(WaterContainer* container);

    ~ProcessTransform() override = default;

    static void RegisterProcessParametersAndForcing(SettingsModel* modelSettings);

    /**
     * @copydoc Process::IsOk()
     */
    bool IsOk() override;

    int GetConnectionsNb() override;

    double* GetValuePointer(const string& name) override;

  protected:
  private:
};

#endif  // HYDROBRICKS_PROCESS_TRANSFORM_H
