#ifndef HYDROBRICKS_PROCESS_ET_HBV_H
#define HYDROBRICKS_PROCESS_ET_HBV_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessET.h"

/**
 * HBV actual evapotranspiration (Lindström et al., 1997).
 *
 * Lives on the soil moisture brick (capacity FC). The actual evaporation
 * increases linearly with the soil moisture and reaches the (corrected) potential
 * rate at the limit LP (expressed as a fraction of FC):
 *   Ea = cevpf × PET × min(SM / (LP × FC), 1)
 *
 * The evaporation correction factor cevpf (default 1) scales the potential
 * evaporation per land cover, allowing e.g. a higher evaporation over forests.
 */
class ProcessETHBV : public ProcessET {
  public:
    explicit ProcessETHBV(WaterContainer* container);

    ~ProcessETHBV() override = default;

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
    Forcing* _pet;                     // non-owning reference
    const float* _lp;                  // soil moisture fraction above which ET reaches the potential rate [-]
    const float* _etCorrectionFactor;  // evaporation correction factor (cevpf) [-]

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_ET_HBV_H
