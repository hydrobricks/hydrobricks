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
    [[nodiscard]] bool IsOk() override;

    /**
     * @copydoc Process::GetConnectionCount()
     */
    int GetConnectionCount() override;

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
     * Get the area fraction of the origin land cover.
     *
     * @return The area fraction of the origin land cover.
     */
    double GetOriginLandCoverAreaFraction();

    /**
     * Get the area fraction of the target land cover.
     *
     * @return The area fraction of the target land cover.
     */
    double GetTargetLandCoverAreaFraction(Flux* flux);

    /**
     * Compute the fraction of areas for the lateral process.
     *
     * @param flux pointer to the flux.
     * @return The computed fraction of areas.
     */
    double ComputeFractionAreas(Flux* flux);

    /**
     * Check if the process is a lateral process.
     *
     * @return true if the process is a lateral process.
     */
    [[nodiscard]] bool IsLateralProcess() const override {
        return true;
    }

  protected:
    vecDouble _weights;
};

#endif  // HYDROBRICKS_PROCESS_LATERAL_H
