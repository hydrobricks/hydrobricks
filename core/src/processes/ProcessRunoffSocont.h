#ifndef HYDROBRICKS_PROCESS_RUNOFF_SOCONT_H
#define HYDROBRICKS_PROCESS_RUNOFF_SOCONT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

class HydroUnit;

class ProcessRunoffSocont : public ProcessOutflow {
  public:
    explicit ProcessRunoffSocont(WaterContainer* container);

    ~ProcessRunoffSocont() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessParametersAndForcing(SettingsModel* modelSettings);

    /**
     * @copydoc Process::SetHydroUnitProperties()
     */
    void SetHydroUnitProperties(HydroUnit* unit, Brick* brick) override;

    /**
     * @copydoc Process::SetParameters()
     */
    void SetParameters(const ProcessSettings& processSettings) override;

  protected:
    float m_slope;           // []
    float* m_beta;           // []
    double* m_areaFraction;  // []
    double m_areaUnit;       // [m^2]
    double m_exponent;

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;

    /**
     * Get the area of the hydro unit.
     *
     * @return The area of the hydro unit in square meters.
     */
    double GetArea();
};

#endif  // HYDROBRICKS_PROCESS_RUNOFF_SOCONT_H
