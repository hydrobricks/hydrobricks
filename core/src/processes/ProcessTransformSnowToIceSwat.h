/**
 * \file
 * This class implements the snow to ice transformation as described in the following paper.
 * Reference: Luo, Y., Arnold, J., Liu, S., Wang, X., & Chen, X. (2013). Inclusion of glacier processes for distributed
 * hydrological modeling at basin scale with application to a watershed in Tianshan Mountains, northwest China.
 * Journal of Hydrology, 477, 72–85. https://doi.org/10.1016/j.jhydrol.2012.11.005
 */

#ifndef HYDROBRICKS_PROCESS_TRANSFORM_SNOWTOICESWAT_H
#define HYDROBRICKS_PROCESS_TRANSFORM_SNOWTOICESWAT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessTransform.h"

class ProcessTransformSnowToIceSwat : public ProcessTransform {
  public:
    explicit ProcessTransformSnowToIceSwat(WaterContainer* container);

    ~ProcessTransformSnowToIceSwat() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessParametersAndForcing(SettingsModel* modelSettings);

    /**
     * @copydoc Process::SetParameters()
     */
    void SetParameters(const ProcessSettings& processSettings) override;

  protected:
    float* _basalAccCoeff;    // Basal accumulation coefficient [-]
    float* _northHemisphere;  // If 1, northern hemisphere (0 for southern hemisphere)

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_TRANSFORM_SNOWTOICESWAT_H
