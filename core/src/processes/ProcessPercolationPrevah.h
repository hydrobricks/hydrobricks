#ifndef HYDROBRICKS_PROCESS_PERCOLATION_PREVAH_H
#define HYDROBRICKS_PROCESS_PERCOLATION_PREVAH_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

class Snowpack;

/**
 * PREVAH percolation: a constant maximum rate gated by the soil moisture state.
 *
 *   PERC = rate                                  if SM >= FC
 *   PERC = rate × kwper × (SM/FC − θ) / (1 − θ)  if θ×FC < SM < FC
 *   PERC = 0                                     if SM <= θ×FC
 *
 * rate is the maximum percolation rate [mm/d], SM and FC the content and capacity
 * of the gate bricks (the soil moisture stores) and θ the soil moisture fraction
 * (PREVAH's CU, shared with the ET limit) below which percolation stops. The
 * percolation draws from the process's own container (the upper zone) but is
 * modulated by the soil state. kwper is PREVAH's hydraulic-conductivity factor
 * (KWPER, 'conductivity_factor'); as in the original, it scales the ramp but not
 * the rate at saturation.
 *
 * PREVAH also percolates at the constant rate × kwper, whatever the soil moisture,
 * where its ET limit (θ times the capacity from the soil map) is at least the
 * capacity the land cover finally imposes: built-up and rock surfaces whose soil map
 * promises more than their fixed 5 mm or 3 mm. That branch is off under snow (SWE
 * above 0.1 mm), where the ET limit above the capacity stops the percolation
 * altogether. 'constant_fraction' is the share of the unit following that branch
 * (0 or 1 for a unit carrying a single land use); the snowpacks given as gate
 * bricks tell its snow-covered part, each weighted by the area of its land cover.
 *
 * With several soil gate bricks (one soil moisture store per land cover), the contents
 * and the capacities are summed before the ratio is taken, which is the
 * area-weighted mean saturation of the hydro unit: the stores are fed by their land
 * cover, whose outgoing fluxes already carry its area fraction, so their contents
 * are expressed over the whole unit.
 */
class ProcessPercolationPrevah : public ProcessOutflow {
  public:
    explicit ProcessPercolationPrevah(WaterContainer* container);

    ~ProcessPercolationPrevah() override = default;

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
     * @copydoc Process::NeedsGateBrickLinking()
     */
    [[nodiscard]] bool NeedsGateBrickLinking() const override {
        return true;
    }

    /**
     * @copydoc Process::AddGateBrick()
     */
    void AddGateBrick(Brick* brick) override;

  protected:
    vector<Brick*> _gateBricks;        // non-owning references to the soil moisture stores
    vector<Snowpack*> _snowpacks;      // non-owning references to the unit's snowpacks
    const float* _rate;                // maximum percolation rate [mm/d]
    const float* _thresholdFraction;   // soil moisture fraction below which percolation stops [-]
    const float* _conductivityFactor;  // hydraulic-conductivity factor (PREVAH's KWPER) [-]
    const float* _constantFraction;    // share of the unit percolating at the constant rate [-]

    /**
     * Snow-covered share of the unit, from the snowpack gate bricks (0 without any).
     */
    [[nodiscard]] double GetSnowCoveredFraction() const;

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_PERCOLATION_PREVAH_H
