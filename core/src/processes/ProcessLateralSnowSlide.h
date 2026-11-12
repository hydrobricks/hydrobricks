/**
 * \file
 * This class implements the SnowSlide method as described in the following paper.
 * Reference: Bernhardt, M., & Schulz, K. (2010). SnowSlide: A simple routine for calculating gravitational snow
 * transport. Geophysical Research Letters, 37(11), 1–6. https://doi.org/10.1029/2010GL043086
 * The holding-depth law (coeff = 3178.4, exp = -1.998, slope clamped to 10 degrees, 20 m cap) is not printed in the
 * paper: it comes from the SnowSlide module of the Canadian Hydrological Model (Marsh, C. B., Pomeroy, J. W., &
 * Wheater, H. S. (2020). The Canadian Hydrological Model (CHM) v1.0. Geoscientific Model Development, 13, 225–247.
 * https://doi.org/10.5194/gmd-13-225-2020), without its cos(slope) factor (the depth is taken as vertical).
 * The minimum holding depth is the paper's 5 cm SWE, as a snow depth. Based on the following code:
 * - https://github.com/Chrismarsh/CHM/blob/develop/src/modules/snow_slide.cpp
 * - https://github.com/bergjus94/RavenHydro/blob/Snow_redistribution/src/SnowRedistribution.cpp
 */

#ifndef HYDROBRICKS_PROCESS_LATERAL_SNOWSLIDE_H
#define HYDROBRICKS_PROCESS_LATERAL_SNOWSLIDE_H

#include "ProcessLateral.h"

class FluxToBrick;

/**
 * Gravitational snow redistribution (SnowSlide; Bernhardt & Schulz, 2010; Marsh et al., 2020).
 *
 * Moves snow exceeding a slope-dependent holding capacity to downslope hydro
 * units. The holding threshold (as a snow depth) decreases with the slope:
 *   D_hold = coeff × slope^exp    (clamped to [min_snow_holding_depth, max_snow_depth])
 *
 * The excess SWE (depth above D_hold, converted back to SWE) is distributed over
 * the connected outputs by their weights and target area fractions, capped at
 * 1000 mm and skipped where the target snowpack is already over-accumulated.
 */
class ProcessLateralSnowSlide : public ProcessLateral {
  public:
    explicit ProcessLateralSnowSlide(WaterContainer* container);

    ~ProcessLateralSnowSlide() override = default;

    /**
     * @copydoc Process::IsValid()
     */
    [[nodiscard]] bool IsValid() const override;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessSettings(SettingsModel* modelSettings);

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
    [[nodiscard]] bool IsLateralProcess() const noexcept override {
        return true;
    }

  protected:
    float _slopeDeg;                    // Slope of the hydro unit [°]
    const float* _coeff;                // Coefficient in the equation []
    const float* _exp;                  // Exponent  in the equation []
    const float* _minSlope;             // Minimum slope for snow holding [°]
    const float* _maxSlope;             // Maximum slope for snow holding [°]
    const float* _minSnowHoldingDepth;  // Minimum snow holding depth (when slope > maxSlope) [mm]
    const float* _maxSnowDepth;         // Maximum snow depth (not in the original method) [mm]

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;

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
