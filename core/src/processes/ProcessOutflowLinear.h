#ifndef HYDROBRICKS_PROCESS_OUTFLOW_LINEAR_H
#define HYDROBRICKS_PROCESS_OUTFLOW_LINEAR_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * Linear reservoir outflow.
 *
 *   outflow = k × S
 *
 * k is the response factor [1/d] and S the storage content.
 */
class ProcessOutflowLinear : public ProcessOutflow {
  public:
    explicit ProcessOutflowLinear(WaterContainer* container);

    ~ProcessOutflowLinear() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessSettings(SettingsModel* modelSettings);

    /**
     * @copydoc Process::SetParameters()
     */
    void SetParameters(const ProcessSettings& processSettings) override;

    /**
     * @copydoc Process::HasLinearResponse()
     */
    [[nodiscard]] bool HasLinearResponse() const override {
        return true;
    }

    /**
     * @copydoc Process::GetLinearResponseRate()
     */
    [[nodiscard]] double GetLinearResponseRate() const override {
        assert(_responseFactor);
        return *_responseFactor;
    }

  protected:
    const float* _responseFactor;  // [1/d]

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_LINEAR_H
