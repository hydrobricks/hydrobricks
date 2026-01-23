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
     * @copydoc Process::IsValid()
     */
    [[nodiscard]] bool IsValid() const override;

    /**
     * @copydoc Process::GetConnectionCount()
     */
    int GetConnectionCount() const override;

    /**
     * @copydoc Process::GetValuePointer()
     */
    double* GetValuePointer(const string& name) override;

    /**
     * @copydoc Process::ToAtmosphere()
     */
    [[nodiscard]] bool ToAtmosphere() const override {
        return true;
    }
};

#endif  // HYDROBRICKS_PROCESS_ET_H
