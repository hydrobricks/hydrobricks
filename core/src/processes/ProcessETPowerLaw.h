#ifndef HYDROBRICKS_PROCESS_ET_POWER_LAW_H
#define HYDROBRICKS_PROCESS_ET_POWER_LAW_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessET.h"

/**
 * Power-law actual evapotranspiration.
 *
 * Lives on a storage brick with a maximum capacity. The actual evaporation scales
 * with the storage filling ratio raised to a calibratable exponent:
 *   Ea = PET × (S/S_max)^exponent
 *
 * Generalizes the Socont ET (exponent fixed at 0.5) and includes the linear case
 * (exponent = 1) as well as the convex (exponent > 1) and concave (exponent < 1)
 * families. Requires PET forcing.
 */
class ProcessETPowerLaw : public ProcessET {
  public:
    explicit ProcessETPowerLaw(WaterContainer* container);

    ~ProcessETPowerLaw() override = default;

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
    Forcing* _pet;           // non-owning reference
    const float* _exponent;  // exponent applied to the filling ratio [-]

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_ET_POWER_LAW_H
