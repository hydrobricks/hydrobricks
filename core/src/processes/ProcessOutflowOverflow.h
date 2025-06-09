#ifndef HYDROBRICKS_PROCESS_OVERFLOW_H
#define HYDROBRICKS_PROCESS_OVERFLOW_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

class ProcessOutflowOverflow : public ProcessOutflow {
  public:
    explicit ProcessOutflowOverflow(WaterContainer* container);

    ~ProcessOutflowOverflow() override = default;

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

    /**
     * @copydoc Process::StoreInOutgoingFlux()
     */
    void StoreInOutgoingFlux(double* rate, int index) override;

  protected:
    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_OVERFLOW_H
