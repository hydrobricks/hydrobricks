#ifndef HYDROBRICKS_PROCESS_MELT_H
#define HYDROBRICKS_PROCESS_MELT_H

#include "Forcing.h"
#include "Includes.h"
#include "Process.h"

class ProcessMelt : public Process {
  public:
    explicit ProcessMelt(Brick* brick);

    ~ProcessMelt() override = default;

    /**
     * @copydoc Process::IsOk()
     */
    bool IsOk() override;

    int GetConnectionsNb() override;

    double* GetValuePointer(const wxString& name) override;

  protected:

  private:
};

#endif  // HYDROBRICKS_PROCESS_MELT_H
