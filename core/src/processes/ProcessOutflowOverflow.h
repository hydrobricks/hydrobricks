#ifndef HYDROBRICKS_PROCESS_OVERFLOW_H
#define HYDROBRICKS_PROCESS_OVERFLOW_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * Overflow of a storage with a maximum capacity.
 *
 * Releases the water exceeding the storage capacity. The excess is not computed
 * here as a rate (GetRates() returns 0); the overflowing amount is handled at a
 * later stage by the water container once the capacity is exceeded, and routed
 * through the linked outgoing flux.
 */
class ProcessOutflowOverflow : public ProcessOutflow {
  public:
    explicit ProcessOutflowOverflow(WaterContainer* container);

    ~ProcessOutflowOverflow() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessSettings(SettingsModel* modelSettings);

    /**
     * @copydoc Process::SetParameters()
     */
    void SetParameters(const ProcessSettings& processSettings) override;

    /**
     * @copydoc Process::StoreInOutgoingFlux()
     */
    void StoreInOutgoingFlux(double* rate, int index) override;

  protected:
    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_OVERFLOW_H
