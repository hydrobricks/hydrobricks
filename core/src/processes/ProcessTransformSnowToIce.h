#ifndef HYDROBRICKS_PROCESS_TRANSFORM_SNOWTOICE_H
#define HYDROBRICKS_PROCESS_TRANSFORM_SNOWTOICE_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessTransform.h"

class ProcessTransformSnowToIce : public ProcessTransform {
  public:
    explicit ProcessTransformSnowToIce(WaterContainer* container);

    ~ProcessTransformSnowToIce() override = default;

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

#endif  // HYDROBRICKS_PROCESS_TRANSFORM_SNOWTOICE_H
