#ifndef HYDROBRICKS_PROCESS_INFILTRATION_HBV_H
#define HYDROBRICKS_PROCESS_INFILTRATION_HBV_H

#include "Includes.h"
#include "ProcessInfiltration.h"

/**
 * HBV soil-moisture infiltration (beta function; Lindström et al., 1997).
 *
 * Lives on the ground land cover (zero storage), which receives rain and
 * snowpack outflow. The incoming water is split according to the soil moisture
 * state of the target brick (soil_moisture, capacity FC):
 *   infiltration = in × (1 − (SM/FC)^beta)
 *
 * The complement, in × (SM/FC)^beta, is the recharge of the response routine
 * and must be routed by a subsequent outflow:rest process (declared after this
 * one) to the upper zone.
 *
 * Requires a linked target brick (soil_moisture) to read its filling ratio.
 */
class ProcessInfiltrationHBV : public ProcessInfiltration {
  public:
    explicit ProcessInfiltrationHBV(WaterContainer* container);

    ~ProcessInfiltrationHBV() override = default;

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
    const float* _beta;  // shape coefficient of the recharge function [-]

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_INFILTRATION_HBV_H
