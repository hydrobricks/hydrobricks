#ifndef HYDROBRICKS_PROCESS_TRANSFORM_SNOWTOICE_H
#define HYDROBRICKS_PROCESS_TRANSFORM_SNOWTOICE_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessTransform.h"

class ProcessTransformSnowToIce : public ProcessTransform {
  public:
    explicit ProcessTransformSnowToIce(WaterContainer* container);

    ~ProcessTransformSnowToIce() override = default;

    static void RegisterProcessParametersAndForcing(SettingsModel* modelSettings);

    /**
     * @copydoc Process::SetParameters()
     */
    void SetParameters(const ProcessSettings& processSettings) override;

  protected:
    float* m_rate;  // [mm/d]

    vecDouble GetRates() override;

  private:
};

#endif  // HYDROBRICKS_PROCESS_TRANSFORM_SNOWTOICE_H
