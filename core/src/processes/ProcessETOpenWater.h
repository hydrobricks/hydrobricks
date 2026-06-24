#ifndef HYDROBRICKS_PROCESS_ET_OPEN_WATER_H
#define HYDROBRICKS_PROCESS_ET_OPEN_WATER_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessET.h"

/**
 * Open-water (potential) evaporation.
 *
 * Evaporates at the potential rate, capped at the available content over the time step
 * so the store cannot go negative:
 *   Ea = min(PET, content / dt)
 *
 * Used for open water (lakes) and for the canopy evaporation of a forest interception
 * store. Requires PET forcing.
 */
class ProcessETOpenWater : public ProcessET {
  public:
    explicit ProcessETOpenWater(WaterContainer* container);

    ~ProcessETOpenWater() override = default;

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

#endif  // HYDROBRICKS_PROCESS_ET_OPEN_WATER_H
