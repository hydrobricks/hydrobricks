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
    bool IsOk() override;

    int GetConnectionsNb() override;

    double* GetValuePointer(const std::string& name) override;

  protected:
  private:
};

#endif  // HYDROBRICKS_PROCESS_MELT_H
