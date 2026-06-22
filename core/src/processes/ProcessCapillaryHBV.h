#ifndef HYDROBRICKS_PROCESS_CAPILLARY_HBV_H
#define HYDROBRICKS_PROCESS_CAPILLARY_HBV_H

#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * HBV-96 capillary transport (Lindström et al., 1997).
 *
 * Lives on the (shared) upper zone and returns water to the soil moisture
 * brick(s) (capacity FC) proportionally to the soil moisture deficit:
 *   CF = cflux × (1 − SM/FC)
 *
 * With several per-class soils the flux fans out to each soil, area-weighted by
 * the land cover(s) that feed it: the rate toward soil i is
 *   CF_i = cflux × w_i × (1 − SM_i/FC_i),   w_i = Σ land-cover area fractions
 * feeding soil i (so the total leaving the upper zone is the area-weighted sum).
 * The upper zone is a full-unit-area brick, so the weighting is carried in the
 * rate (the fluxes are unweighted) to keep the water balance exact. With a single
 * soil-bearing cover (w = 1) this reduces to the original single-target flux.
 *
 * With cflux = 0 (the default) the process is inactive. Requires linked target
 * brick(s) (soil moisture) to read their filling ratio.
 */
class ProcessCapillaryHBV : public ProcessOutflow {
  public:
    explicit ProcessCapillaryHBV(WaterContainer* container);

    ~ProcessCapillaryHBV() override = default;

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

    /**
     * @copydoc Process::NeedsTargetBrickLinking()
     */
    [[nodiscard]] bool NeedsTargetBrickLinking() const override {
        return true;
    }

    /**
     * @copydoc Process::LinksMultipleTargets()
     */
    [[nodiscard]] bool LinksMultipleTargets() const override {
        return true;
    }

    /**
     * @copydoc Process::AddTargetBrickWithWeights()
     */
    void AddTargetBrickWithWeights(Brick* targetBrick, const std::vector<Brick*>& weightSources) override;

  protected:
    std::vector<Brick*> _targetBricks;                 // soil bricks (non-owning)
    std::vector<std::vector<const double*>> _weights;  // live land-cover area fractions per target
    const float* _maxCapillaryFlux;                    // cflux [mm/d]

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_CAPILLARY_HBV_H
