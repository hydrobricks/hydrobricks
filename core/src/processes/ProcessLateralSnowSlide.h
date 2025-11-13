/**
 * \file
 * This class implements the SnowSlide method as described in the following paper.
 * Reference: Bernhardt, M., & Schulz, K. (2010). SnowSlide: A simple routine for calculating gravitational snow
 * transport. Geophysical Research Letters, 37(11), 1–6. https://doi.org/10.1029/2010GL043086 Based on the following
 * code:
 * - https://github.com/Chrismarsh/CHM/blob/develop/src/modules/snow_slide.cpp
 * - https://github.com/bergjus94/RavenHydro/blob/Snow_redistribution/src/SnowRedistribution.cpp
 */

#ifndef HYDROBRICKS_PROCESS_LATERAL_SNOWSLIDE_H
#define HYDROBRICKS_PROCESS_LATERAL_SNOWSLIDE_H

#include "ProcessLateral.h"

class FluxToBrick;

class ProcessLateralSnowSlide : public ProcessLateral {
  public:
    explicit ProcessLateralSnowSlide(WaterContainer* container);

    ~ProcessLateralSnowSlide() override = default;

    /**
     * @copydoc Process::IsOk()
     */
    [[nodiscard]] bool IsOk() override;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessParametersAndForcing(SettingsModel* modelSettings);

    /**
     * @copydoc Process::SetHydroUnitProperties()
     */
    void SetHydroUnitProperties(HydroUnit* unit, Brick* brick) override;

    /**
     * @copydoc Process::SetParameters()
     */
    void SetParameters(const ProcessSettings& processSettings) override;

    /**
     * @copydoc Process::IsLateralProcess()
     */
    [[nodiscard]] bool IsLateralProcess() const override {
        return true;
    }

  protected:
    float _slope_deg;             // Slope of the hydro unit [°]
    float* _coeff;                // Coefficient in the equation []
    float* _exp;                  // Exponent  in the equation []
    float* _minSlope;             // Minimum slope for snow holding [°]
    float* _maxSlope;             // Maximum slope for snow holding [°]
    float* _minSnowHoldingDepth;  // Minimum snow holding depth (when slope > maxSlope) [mm]
    float* _maxSnowDepth;         // Maximum snow depth (not in the original method) [mm]

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;

  private:
    /**
     * Avoid unrealistic accumulation rates by not redistributing snow if the target snowpack has more than twice the
     * overall maximum snow depth.
     *
     * @param rate The rate to check.
     * @param flux The flux to the target brick.
     * @return the corrected rate.
     */
    double AvoidUnrealisticAccumulation(double rate, Flux* flux);
};

#endif  // HYDROBRICKS_PROCESS_LATERAL_SNOWSLIDE_H
