#ifndef HYDROBRICKS_PROCESS_MELT_TEMPERATURE_INDEX_H
#define HYDROBRICKS_PROCESS_MELT_TEMPERATURE_INDEX_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessMelt.h"

/**
 * Enhanced temperature-index melt with a radiation term (Hock, 1999).
 *
 * Adds a solar-radiation contribution to the degree-day melt:
 *   M = (T − TT) × (MF + r × I)    for T ≥ TT, else 0
 *
 * MF is the melt factor, r the radiation coefficient and I the incoming solar
 * radiation. Requires temperature and solar_radiation forcings.
 */
class ProcessMeltTemperatureIndex : public ProcessMelt {
  public:
    explicit ProcessMeltTemperatureIndex(WaterContainer* container);

    ~ProcessMeltTemperatureIndex() override = default;

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
     * @copydoc Process::AttachForcing()
     */
    void AttachForcing(Forcing* forcing) override;

  protected:
    Forcing* _temperature;     // non-owning reference
    Forcing* _solarRadiation;  // non-owning reference
    const float* _meltFactor;
    const float* _meltingTemperature;
    const float* _radiationCoefficient;

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_MELT_TEMPERATURE_INDEX_H
