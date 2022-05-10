#ifndef HYDROBRICKS_PROCESS_OUTFLOW_H
#define HYDROBRICKS_PROCESS_OUTFLOW_H

#include "Forcing.h"
#include "Includes.h"
#include "Process.h"

class ProcessOutflow : public Process {
  public:
    explicit ProcessOutflow(Brick* brick);

    ~ProcessOutflow() override = default;

    /**
     * @copydoc Process::IsOk()
     */
    bool IsOk() override;

    double* GetValuePointer(const wxString& name) override;

  protected:

  private:
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_H
