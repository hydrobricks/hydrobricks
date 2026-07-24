#ifndef HYDROBRICKS_PROCESS_SUBLIMATION_PET_H
#define HYDROBRICKS_PROCESS_SUBLIMATION_PET_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessSublimation.h"

/**
 * PET-fraction snow sublimation.
 *
 * Treats snow sublimation as the cold-surface analog of evaporation, driven by
 * atmospheric demand ("evapo-sublimation", Herrero & Polo, 2016): a fixed
 * fraction of the potential evapotranspiration is removed from the snowpack:
 *   S = pet_factor × PET
 *
 * This mirrors the fraction-of-PET snow-evaporation schemes used in simple
 * glacio-hydrological models. Requires PET forcing; no sublimation when the
 * container is empty.
 */
class ProcessSublimationPET : public ProcessSublimation {
  public:
    explicit ProcessSublimationPET(WaterContainer* container);

    ~ProcessSublimationPET() override = default;

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
    Forcing* _pet;  // non-owning reference
    const float* _petFactor;

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_SUBLIMATION_PET_H
