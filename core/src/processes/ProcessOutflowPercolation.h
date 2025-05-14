#ifndef HYDROBRICKS_PROCESS_OUTFLOW_PERCOLATION_H
#define HYDROBRICKS_PROCESS_OUTFLOW_PERCOLATION_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

class ProcessOutflowPercolation : public ProcessOutflow {
  public:
    explicit ProcessOutflowPercolation(WaterContainer* container);

    ~ProcessOutflowPercolation() override = default;

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
    float* m_rate;  // [mm/d]

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_PERCOLATION_H
