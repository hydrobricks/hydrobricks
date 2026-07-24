/**
 * \file
 * This class implements the conceptual snow redistribution method described in the following paper.
 * Reference: Frey, S., & Holzmann, H. (2015). A conceptual, distributed snow redistribution model.
 * Hydrology and Earth System Sciences, 19(11), 4517-4530. https://doi.org/10.5194/hess-19-4517-2015
 */

#ifndef HYDROBRICKS_PROCESS_LATERAL_SNOW_REDISTRIBUTION_FREY_H
#define HYDROBRICKS_PROCESS_LATERAL_SNOW_REDISTRIBUTION_FREY_H

#include "ProcessLateral.h"

class FluxToBrick;
class Forcing;

/**
 * Conceptual snow redistribution (Frey & Holzmann, 2015).
 *
 * Moves snow exceeding a land-cover-based holding capacity (H_v) to downslope hydro units. The
 * redistributed amount is scaled by a distribution coefficient f_rho that depends on the snow density
 * and the slope angle, and by a calibratable correction coefficient C (paper Eqs. 12-13). Working in SWE
 * (the only quantity the snowpack stores), with H_v given as a snow-depth threshold:
 *   excess        = max(swe - H_v / sweToDepthFactor, 0)
 *   f_rho         = ((rhoMax - rho) / rhoMax) * exp(-rho / rhoMax) * (slope / 90)
 *   redistributed = excess * f_rho * C
 *
 * The excess SWE is distributed over the connected outputs by their weights (the lateral-connection
 * fractions, which encode the 1/SUM(A) split among acceptors) and target area fractions, capped at
 * 1000 mm and skipped where the target snowpack is already over-accumulated.
 *
 * Two snow-density variants are supported:
 *  - constant: rho is a fixed parameter (snow_density); f_rho then depends only on the slope.
 *  - dynamic: rho is tracked per snowpack and evolves with air temperature (fresh-snow density,
 *    paper Eqs. 8-9) and settling toward rhoMax (a simplification of Eq. 10, since hydrobricks does
 *    not track the liquid water content the original settling relation uses). The density state is
 *    advanced exactly once per time step in Finalize().
 */
class ProcessLateralSnowRedistributionFrey : public ProcessLateral {
  public:
    explicit ProcessLateralSnowRedistributionFrey(WaterContainer* container, bool dynamicDensity = false);

    ~ProcessLateralSnowRedistributionFrey() override = default;

    /**
     * @copydoc Process::IsValid()
     */
    [[nodiscard]] bool IsValid() const override;

    /**
     * Register the process parameters for the constant-density variant.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessSettings(SettingsModel* modelSettings);

    /**
     * Register the process parameters and forcing for the dynamic-density variant.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessSettingsDynamic(SettingsModel* modelSettings);

    /**
     * @copydoc Process::SetHydroUnitProperties()
     */
    void SetHydroUnitProperties(HydroUnit* unit, Brick* brick) override;

    /**
     * @copydoc Process::SetParameters()
     */
    void SetParameters(const ProcessSettings& processSettings) override;

    /**
     * @copydoc Process::AttachForcing()
     */
    void AttachForcing(Forcing* forcing) override;

    /**
     * @copydoc Process::Reset()
     */
    void Reset() override;

    /**
     * @copydoc Process::Finalize()
     */
    void Finalize() override;

    /**
     * @copydoc Process::IsLateralProcess()
     */
    [[nodiscard]] bool IsLateralProcess() const noexcept override {
        return true;
    }

  protected:
    bool _dynamicDensity;           // Whether the snow density evolves dynamically
    float _slopeDeg;                // Slope of the hydro unit [°]
    Forcing* _temperature;          // Air temperature forcing (dynamic variant only)
    const float* _correction;       // Correction coefficient C []
    const float* _holdingCapacity;  // Land-cover snow holding capacity H_v (as a snow depth) [mm]
    const float* _rhoMax;           // Maximum snow density for redistribution [kg/m3]
    const float* _maxSnowDepth;     // Maximum snow depth (over-accumulation guard) [mm]
    const float* _snowDensity;      // Constant snow density (constant variant) [kg/m3]
    const float* _rhoMin;           // Minimum snow density (dynamic variant) [kg/m3]
    const float* _rhoFreshMax;      // Maximum fresh-snow density (dynamic variant) [kg/m3]
    const float* _rhoSettling;      // Settling rate toward rhoMax (dynamic variant) [1/day]
    const float* _rhoScale;         // Density sigmoid slope coefficient (dynamic variant) []
    const float* _tScale;           // Density sigmoid shift coefficient (dynamic variant) [°C]
    double _density;                // Tracked bulk snow density (dynamic variant) [kg/m3]
    double _prevSwe;                // Committed SWE at the previous time step (dynamic variant) [mm]

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;

  private:
    /**
     * Get the snow density to use for the current time step. For the constant variant, returns the
     * snow_density parameter; for the dynamic variant, returns the tracked bulk density.
     *
     * @return the snow density [kg/m3].
     */
    [[nodiscard]] double GetSnowDensity() const;

    /**
     * Compute the fresh-snow density from the air temperature (paper Eqs. 8-9).
     *
     * @return the fresh-snow density [kg/m3].
     */
    [[nodiscard]] double ComputeFreshSnowDensity() const;

    /**
     * Avoid unrealistic accumulation by not redistributing snow if the target snowpack already holds
     * more than 1.5 times the overall maximum snow depth.
     *
     * @param rate The rate to check.
     * @param flux The flux to the target brick.
     * @return the corrected rate.
     */
    double AvoidUnrealisticAccumulation(double rate, Flux* flux);
};

#endif  // HYDROBRICKS_PROCESS_LATERAL_SNOW_REDISTRIBUTION_FREY_H
