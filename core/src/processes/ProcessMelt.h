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

    double* GetValuePointer(const string& name) override;

  protected:
    std::shared_ptr<float> m_meltFactor;

  private:
};

#endif  // HYDROBRICKS_PROCESS_MELT_H
