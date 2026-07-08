#ifndef HYDROBRICKS_PROCESS_OUTFLOW_LINEAR_THRESHOLD_H
#define HYDROBRICKS_PROCESS_OUTFLOW_LINEAR_THRESHOLD_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * Linear reservoir outflow above a storage threshold (PREVAH surface runoff).
 *
 *   outflow = k × max(0, S − θ)
 *
 * k is the response factor [1/d], S the storage content [mm] and θ the storage
 * threshold [mm] below which no outflow occurs. With θ = 0 it reduces to a
 * linear reservoir. This is the PREVAH upper-zone fast runoff
 * Q0 = K0 × (SUZ − SGRLUZ).
 */
class ProcessOutflowLinearThreshold : public ProcessOutflow {
  public:
    explicit ProcessOutflowLinearThreshold(WaterContainer* container);

    ~ProcessOutflowLinearThreshold() override = default;

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

  protected:
    const float* _responseFactor;  // [1/d]
    const float* _threshold;       // storage threshold below which no outflow occurs [mm]

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_LINEAR_THRESHOLD_H
