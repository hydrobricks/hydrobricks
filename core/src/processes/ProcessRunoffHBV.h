#ifndef HYDROBRICKS_PROCESS_RUNOFF_HBV_H
#define HYDROBRICKS_PROCESS_RUNOFF_HBV_H

#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * HBV-96 upper-zone runoff (Lindström et al., 1997).
 *
 * Non-linear outflow of the upper response box:
 *   Q0 = k × UZ^(1 + alpha)
 *
 * With alpha = 0 the storage behaves as a linear reservoir. The response
 * factor k is expressed in [1/d] for alpha = 0 (more generally
 * [mm^(-alpha)/d]).
 */
class ProcessRunoffHBV : public ProcessOutflow {
  public:
    explicit ProcessRunoffHBV(WaterContainer* container);

    ~ProcessRunoffHBV() override = default;

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
    const float* _responseFactor;  // k [mm^(-alpha)/d]
    const float* _alpha;           // non-linearity coefficient [-]

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_RUNOFF_HBV_H
