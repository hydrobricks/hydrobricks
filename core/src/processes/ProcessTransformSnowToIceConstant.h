#ifndef HYDROBRICKS_PROCESS_TRANSFORM_SNOWTOICECONSTANT_H
#define HYDROBRICKS_PROCESS_TRANSFORM_SNOWTOICECONSTANT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessTransform.h"

/**
 * Constant-rate snow-to-ice transformation.
 *
 *   transformation = rate    [mm/d]
 *
 * Converts snow to ice at a fixed rate on a glacier brick, constrained by the
 * available snow content via the container.
 */
class ProcessTransformSnowToIceConstant : public ProcessTransform {
  public:
    explicit ProcessTransformSnowToIceConstant(WaterContainer* container);

    ~ProcessTransformSnowToIceConstant() override = default;

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

  protected:
    const float* _rate;  // [mm/d]

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_TRANSFORM_SNOWTOICECONSTANT_H
