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
     * @copydoc Process::IsOk()
     */
    bool IsOk() override;

    int GetConnectionsNb() override;

    double* GetValuePointer(const string& name) override;

  protected:
  private:
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_H
