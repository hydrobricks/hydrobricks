#ifndef HYDROBRICKS_PROCESS_ET_H
#define HYDROBRICKS_PROCESS_ET_H

#include "Forcing.h"
#include "Includes.h"
#include "Process.h"

class ProcessET : public Process {
  public:
    explicit ProcessET(WaterContainer* container);

    ~ProcessET() override = default;

    /**
     * @copydoc Process::IsOk()
     */
    [[nodiscard]] bool IsOk() override;

    /**
     * @copydoc Process::GetConnectionCount()
     */
    int GetConnectionCount() override;

    /**
     * @copydoc Process::GetValuePointer()
     */
    double* GetValuePointer(const string& name) override;

    /**
     * @copydoc Process::ToAtmosphere()
     */
    [[nodiscard]] bool ToAtmosphere() override {
        return true;
    }
};

#endif  // HYDROBRICKS_PROCESS_ET_H
