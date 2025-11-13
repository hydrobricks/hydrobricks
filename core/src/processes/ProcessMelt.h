#ifndef HYDROBRICKS_PROCESS_MELT_H
#define HYDROBRICKS_PROCESS_MELT_H

#include "Forcing.h"
#include "Includes.h"
#include "Process.h"

class ProcessMelt : public Process {
  public:
    explicit ProcessMelt(WaterContainer* container);

    ~ProcessMelt() override = default;

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

#endif  // HYDROBRICKS_PROCESS_MELT_H
