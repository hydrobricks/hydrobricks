#ifndef HYDROBRICKS_PROCESS_LATERAL_H
#define HYDROBRICKS_PROCESS_LATERAL_H

#include "Forcing.h"
#include "Includes.h"
#include "Process.h"

class ProcessLateral : public Process {
  public:
    explicit ProcessLateral(WaterContainer* container);

    ~ProcessLateral() override = default;

    /**
     * @copydoc Process::IsOk()
     */
    bool IsOk() override;

    /**
     * @copydoc Process::GetConnectionsNb()
     */
    int GetConnectionsNb() override;

    /**
     * @copydoc Process::GetValuePointer()
     */
    double* GetValuePointer(const string& name) override;

    /**
     * Attach outgoing flux.
     *
     * @param flux outgoing flux
     * @param weight weight of the flux
     */
    void AttachFluxOutWithWeight(Flux* flux, double weight = 1.0);

    /**
     * Check if the process is a lateral process.
     *
     * @return true if the process is a lateral process.
     */
    bool IsLateralProcess() const override {
        return true;
    }

  protected:
    vecDouble _weights;
};

#endif  // HYDROBRICKS_PROCESS_LATERAL_H
