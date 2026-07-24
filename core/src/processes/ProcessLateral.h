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
     * Attach outgoing flux.
     *
     * @param flux outgoing flux (ownership transferred)
     * @param weight weight of the flux
     */
    void AttachFluxOutWithWeight(std::unique_ptr<Flux> flux, double weight = 1.0);

    /**
     * Get the area fraction of the origin land cover.
     *
     * @return The area fraction of the origin land cover.
     */
    [[nodiscard]] double GetOriginLandCoverAreaFraction() const;

    /**
     * Get the area fraction of the target land cover.
     *
     * @return The area fraction of the target land cover.
     */
    [[nodiscard]] double GetTargetLandCoverAreaFraction(Flux* flux);

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
    [[nodiscard]] bool IsLateralProcess() const noexcept override {
        return true;
    }

  protected:
    vecDouble _weights;
};

#endif  // HYDROBRICKS_PROCESS_LATERAL_H
