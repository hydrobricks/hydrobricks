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
    bool IsOk() override;

    int GetConnectionsNb() override;

    double* GetValuePointer(const wxString& name) override;

    bool ToAtmosphere() override {
        return true;
    }

  protected:

  private:
};

#endif  // HYDROBRICKS_PROCESS_ET_H
