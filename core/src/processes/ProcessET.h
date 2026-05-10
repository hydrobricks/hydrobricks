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
    [[nodiscard]] int GetConnectionCount() const override;

    /**
     * @copydoc Process::GetValuePointer()
     */
    double* GetValuePointer(std::string_view name) override;

    /**
     * @copydoc Process::ToAtmosphere()
     */
    [[nodiscard]] bool ToAtmosphere() const override {
        return true;
    }
};

#endif  // HYDROBRICKS_PROCESS_ET_H
