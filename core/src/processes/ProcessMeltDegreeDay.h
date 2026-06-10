#ifndef HYDROBRICKS_PROCESS_MELT_DEGREE_DAY_H
#define HYDROBRICKS_PROCESS_MELT_DEGREE_DAY_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessMelt.h"

class ProcessMeltDegreeDay : public ProcessMelt {
  public:
    explicit ProcessMeltDegreeDay(WaterContainer* container);

    ~ProcessMeltDegreeDay() override = default;

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

    /**
     * Get the degree-day factor value.
     *
     * @return The degree-day factor [mm/d/°C].
     */
    [[nodiscard]] double GetDegreeDayFactor() const {
        assert(_degreeDayFactor);
        return *_degreeDayFactor;
    }

    /**
     * Get the melting temperature value.
     *
     * @return The melting temperature [°C].
     */
    [[nodiscard]] double GetMeltingTemperature() const {
        assert(_meltingTemperature);
        return *_meltingTemperature;
    }

  protected:
    Forcing* _temperature;  // non-owning reference
    const float* _degreeDayFactor;
    const float* _meltingTemperature;

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_MELT_DEGREE_DAY_H
