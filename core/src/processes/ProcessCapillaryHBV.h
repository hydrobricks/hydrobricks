#ifndef HYDROBRICKS_PROCESS_CAPILLARY_HBV_H
#define HYDROBRICKS_PROCESS_CAPILLARY_HBV_H

#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * HBV-96 capillary transport (Lindström et al., 1997).
 *
 * Lives on the upper zone and returns water to the soil moisture brick
 * (capacity FC) proportionally to the soil moisture deficit:
 *   CF = cflux × (1 − SM/FC)
 *
 * With cflux = 0 (the default) the process is inactive. Requires a linked
 * target brick (soil_moisture) to read its filling ratio.
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
     * @copydoc Process::SetTargetBrick()
     */
    void SetTargetBrick(Brick* targetBrick) override {
        _targetBrick = targetBrick;
    }

  protected:
    Brick* _targetBrick;             // non-owning reference
    const float* _maxCapillaryFlux;  // cflux [mm/d]

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_CAPILLARY_HBV_H
