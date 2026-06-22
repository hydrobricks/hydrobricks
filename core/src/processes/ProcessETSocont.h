#ifndef HYDROBRICKS_PROCESS_ET_SOCONT_H
#define HYDROBRICKS_PROCESS_ET_SOCONT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessET.h"

/**
 * Socont actual evapotranspiration.
 *
 * Lives on a storage brick with a maximum capacity. The actual evaporation scales
 * with the storage filling ratio raised to a fixed exponent (0.5):
 *   Ea = PET × (S/S_max)^0.5
 *
 * Requires PET forcing.
 */
class ProcessETSocont : public ProcessET {
  public:
    explicit ProcessETSocont(WaterContainer* container);

    ~ProcessETSocont() override = default;

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
     * @copydoc Process::AttachForcing()
     */
    void AttachForcing(Forcing* forcing) override;

  protected:
    Forcing* _pet;  // non-owning reference
    float _exponent;

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_ET_SOCONT_H
