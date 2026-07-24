#ifndef HYDROBRICKS_PROCESS_ET_LINEAR_H
#define HYDROBRICKS_PROCESS_ET_LINEAR_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessET.h"

/**
 * Linear actual evapotranspiration.
 *
 * Lives on a storage brick with a maximum capacity. The actual evaporation scales
 * linearly with the storage filling ratio and reaches the potential rate when full:
 *   Ea = PET × min(S/S_max, 1)
 *
 * Parameter-free baseline reduction. Requires PET forcing.
 */
class ProcessETLinear : public ProcessET {
  public:
    explicit ProcessETLinear(WaterContainer* container);

    ~ProcessETLinear() override = default;

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

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_ET_LINEAR_H
