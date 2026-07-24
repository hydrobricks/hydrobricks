#ifndef HYDROBRICKS_PROCESS_SUBLIMATION_CONSTANT_H
#define HYDROBRICKS_PROCESS_SUBLIMATION_CONSTANT_H

#include "Includes.h"
#include "ProcessSublimation.h"

/**
 * Constant-rate snow sublimation.
 *
 * Removes snow at a fixed rate whenever snow is present:
 *   S = sublimation_rate    [mm/d]
 *
 * The simplest possible parameterisation (no forcing dependency), a first-order
 * approximation of the near-steady mean daily surface-sublimation rates reported
 * from field measurements in mountain snowpacks (Strasser et al., 2008;
 * Herrero & Polo, 2016). No sublimation when the container is empty.
 */
class ProcessSublimationConstant : public ProcessSublimation {
  public:
    explicit ProcessSublimationConstant(WaterContainer* container);

    ~ProcessSublimationConstant() override = default;

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
    const float* _sublimationRate;

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_SUBLIMATION_CONSTANT_H
