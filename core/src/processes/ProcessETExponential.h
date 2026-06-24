#ifndef HYDROBRICKS_PROCESS_ET_EXPONENTIAL_H
#define HYDROBRICKS_PROCESS_ET_EXPONENTIAL_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessET.h"

/**
 * Exponential actual evapotranspiration.
 *
 * Lives on a storage brick with a maximum capacity. The actual evaporation rises
 * concavely with the storage filling ratio and asymptotes toward the potential rate:
 *   Ea = PET × (1 - exp(-alpha × S/S_max))
 *
 * The curvature parameter alpha controls how quickly AET approaches PET as the store
 * fills (larger alpha = steeper rise, closer to PET when wet). Requires PET forcing.
 */
class ProcessETExponential : public ProcessET {
  public:
    explicit ProcessETExponential(WaterContainer* container);

    ~ProcessETExponential() override = default;

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
    Forcing* _pet;        // non-owning reference
    const float* _alpha;  // curvature of the concave reduction curve [-]

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_ET_EXPONENTIAL_H
