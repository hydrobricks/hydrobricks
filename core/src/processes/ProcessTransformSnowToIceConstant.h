#ifndef HYDROBRICKS_PROCESS_TRANSFORM_SNOWTOICECONSTANT_H
#define HYDROBRICKS_PROCESS_TRANSFORM_SNOWTOICECONSTANT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessTransform.h"

class ProcessTransformSnowToIceConstant : public ProcessTransform {
  public:
    explicit ProcessTransformSnowToIceConstant(WaterContainer* container);

    ~ProcessTransformSnowToIceConstant() override = default;

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
    float* _rate;  // [mm/d]

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_TRANSFORM_SNOWTOICECONSTANT_H
