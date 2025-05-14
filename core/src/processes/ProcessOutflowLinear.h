#ifndef HYDROBRICKS_PROCESS_OUTFLOW_LINEAR_H
#define HYDROBRICKS_PROCESS_OUTFLOW_LINEAR_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

class ProcessOutflowLinear : public ProcessOutflow {
  public:
    explicit ProcessOutflowLinear(WaterContainer* container);

    ~ProcessOutflowLinear() override = default;

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
    float* m_responseFactor;  // [1/d]

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_LINEAR_H
