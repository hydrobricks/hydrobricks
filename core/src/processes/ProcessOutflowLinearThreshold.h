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

    /**
     * @copydoc Process::HasLinearResponse()
     *
     * The response is affine (k × (S − θ)) only while the store is above the
     * threshold; below it the process is simply off, and reporting no linear
     * response keeps it out of the analytic solver's decay coefficient.
     */
    [[nodiscard]] bool HasLinearResponse() const override;

    /**
     * @copydoc Process::GetLinearResponseRate()
     */
    [[nodiscard]] double GetLinearResponseRate() const override {
        assert(_responseFactor);
        return *_responseFactor;
    }

    /**
     * @copydoc Process::GetLinearResponseOffset()
     */
    [[nodiscard]] double GetLinearResponseOffset() const override {
        assert(_responseFactor && _threshold);
        return (*_responseFactor) * (*_threshold);
    }

  protected:
    const float* _responseFactor;  // [1/d]
    const float* _threshold;       // storage threshold below which no outflow occurs [mm]

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_LINEAR_THRESHOLD_H
