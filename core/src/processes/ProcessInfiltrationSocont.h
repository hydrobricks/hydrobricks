#ifndef HYDROBRICKS_PROCESS_INFILTRATION_SOCONT_H
#define HYDROBRICKS_PROCESS_INFILTRATION_SOCONT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessInfiltration.h"

class ProcessInfiltrationSocont : public ProcessInfiltration {
  public:
    explicit ProcessInfiltrationSocont(WaterContainer* container);

    ~ProcessInfiltrationSocont() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessParametersAndForcing(SettingsModel* modelSettings);

    /**
     * @copydoc Process::SetParameters()
     */
    void SetParameters(const ProcessSettings& processSettings) override;

  protected:
    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_INFILTRATION_SOCONT_H
