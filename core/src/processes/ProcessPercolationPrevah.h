#ifndef HYDROBRICKS_PROCESS_PERCOLATION_PREVAH_H
#define HYDROBRICKS_PROCESS_PERCOLATION_PREVAH_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * PREVAH percolation: a constant maximum rate gated by the soil moisture state.
 *
 *   PERC = rate × clamp((SM/FC − θ) / (1 − θ), 0, 1)
 *
 * rate is the maximum percolation rate [mm/d], SM and FC the content and capacity
 * of the gate brick (the soil moisture store) and θ the soil moisture fraction
 * (PREVAH's CU, shared with the ET limit) below which percolation stops. The
 * percolation draws from the process's own container (the upper zone) but is
 * modulated by the soil state: full rate at saturation, linear ramp between
 * θ×FC and FC, none below θ×FC.
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
     * @copydoc Process::SetGateBrick()
     */
    void SetGateBrick(Brick* brick) override {
        _gateBrick = brick;
    }

  protected:
    Brick* _gateBrick;                // non-owning reference to the soil moisture store
    const float* _rate;               // maximum percolation rate [mm/d]
    const float* _thresholdFraction;  // soil moisture fraction below which percolation stops [-]

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_PERCOLATION_PREVAH_H
