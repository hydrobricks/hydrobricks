#ifndef HYDROBRICKS_PROCESS_ET_H
#define HYDROBRICKS_PROCESS_ET_H

#include "Forcing.h"
#include "Includes.h"
#include "Process.h"

class ProcessET : public Process {
  public:
    explicit ProcessET(Brick* brick);

    ~ProcessET() override = default;

    /**
     * @copydoc Process::IsOk()
     */
    bool IsOk() override;

    int GetConnectionsNb() override;

  protected:

  private:
};

#endif  // HYDROBRICKS_PROCESS_ET_H
