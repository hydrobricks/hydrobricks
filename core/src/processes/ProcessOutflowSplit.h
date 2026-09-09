#ifndef HYDROBRICKS_PROCESS_OUTFLOW_SPLIT_H
#define HYDROBRICKS_PROCESS_OUTFLOW_SPLIT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * Fixed-ratio outflow split: drains the container content to two targets.
 *
 *   outflow_1 = f × S / Δt
 *   outflow_2 = (1 − f) × S / Δt
 *
 * f is the split fraction [-] going to the first target. Used as the single
 * process of a pass-through storage to partition an inflow at a fixed ratio
 * (e.g. the PREVAH SLOWCOMP groundwater recharge split of 8/9 to SLZ2 and
 * 1/9 to SLZ3). Because it claims the whole content, it cannot share a
 * container with another process (IsValid() rejects that).
 */
class ProcessOutflowSplit : public ProcessOutflow {
  public:
    explicit ProcessOutflowSplit(WaterContainer* container);

    ~ProcessOutflowSplit() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessSettings(SettingsModel* modelSettings);

    /**
     * @copydoc Process::IsValid()
     *
     * Requires exactly two outputs and, as it drains the whole container content,
     * it is incompatible with any sibling process drawing from the same container.
     */
    [[nodiscard]] bool IsValid() const override;

    /**
     * @copydoc Process::SetParameters()
     */
    void SetParameters(const ProcessSettings& processSettings) override;

    /**
     * @copydoc Process::GetChangeRates()
     *
     * Bypasses the empty-container shortcut of the base class (as ProcessRoutingHBV
     * does): the split typically drains the whole content within the timestep, which
     * would zero the rates at later solver stages. GetRates() clamps at zero content,
     * and the container constraints prevent any negative content.
     */
    [[nodiscard]] const vecDouble& GetChangeRates() override {
        return GetRates();
    }

  protected:
    const float* _splitFraction;  // fraction of the outflow sent to the first target [-]

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_SPLIT_H
