#ifndef HYDROBRICKS_PROCESS_LATERAL_H
#define HYDROBRICKS_PROCESS_LATERAL_H

#include "Forcing.h"
#include "Includes.h"
#include "Process.h"

class ProcessLateral : public Process {
  public:
    explicit ProcessLateral(WaterContainer* container);

    ~ProcessLateral() override = default;

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

#endif  // HYDROBRICKS_PROCESS_LATERAL_H
