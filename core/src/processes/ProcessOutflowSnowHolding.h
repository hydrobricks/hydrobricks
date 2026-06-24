#ifndef HYDROBRICKS_PROCESS_OUTFLOW_SNOW_HOLDING_H
#define HYDROBRICKS_PROCESS_OUTFLOW_SNOW_HOLDING_H

#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * Snowpack liquid water release with a holding capacity (HBV; Lindström et al., 1997).
 *
 * Lives on the water container of a snowpack brick (requires snowpacks
 * generated with water retention). The snowpack retains liquid water (rain on
 * snow and melt water) up to a fraction of the snow water equivalent:
 *   outflow = max(0, W − whc × SWE)
 *
 * The excess is released within the timestep. When no snow is present, all the
 * liquid water is passed through to the target (usually the parent land cover).
 */
class ProcessOutflowSnowHolding : public ProcessOutflow {
  public:
    explicit ProcessOutflowSnowHolding(WaterContainer* container);

    ~ProcessOutflowSnowHolding() override = default;

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
    const float* _waterHoldingCapacity;  // whc, fraction of the SWE [-]

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_SNOW_HOLDING_H
