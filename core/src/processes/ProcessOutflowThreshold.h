#ifndef HYDROBRICKS_PROCESS_OUTFLOW_THRESHOLD_H
#define HYDROBRICKS_PROCESS_OUTFLOW_THRESHOLD_H

#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * Outflow of the content above a fixed capacity threshold.
 *
 * Releases the water held above a capacity within the time step:
 *   outflow = max(0, content − capacity)
 *
 * Unlike the brick-capacity overflow, the threshold is enforced by the process
 * itself (no maximum capacity on the container), so it works correctly on the
 * explicit/direct computation path of a surface component (e.g. a forest canopy,
 * where it produces the throughfall above the interception capacity).
 */
class ProcessOutflowThreshold : public ProcessOutflow {
  public:
    explicit ProcessOutflowThreshold(WaterContainer* container);

    ~ProcessOutflowThreshold() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessSettings(SettingsModel* modelSettings);

    /**
     * @copydoc Process::IsValid()
     */
    [[nodiscard]] bool IsValid() const override;

    /**
     * @copydoc Process::SetParameters()
     */
    void SetParameters(const ProcessSettings& processSettings) override;

  protected:
    const float* _capacity;  // threshold capacity [mm]

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_THRESHOLD_H
