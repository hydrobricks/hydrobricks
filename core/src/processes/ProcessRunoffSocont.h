#ifndef HYDROBRICKS_PROCESS_RUNOFF_SOCONT_H
#define HYDROBRICKS_PROCESS_RUNOFF_SOCONT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

class HydroUnit;

/**
 * Socont surface runoff (kinematic-wave overland flow on an inclined plane).
 *
 * Treats the storage as a thin water layer flowing down a plane of given slope,
 * with a linear depth profile (zero at the top, h at the bottom; storage shape = 2).
 * The outflow follows a Manning-type relation:
 *   q = beta × slope^0.5 × h^(5/3) / area
 * integrated over the timestep and capped at the available content. beta lumps the
 * roughness and geometry; the slope is read from the hydro unit.
 */
class ProcessRunoffSocont : public ProcessOutflow {
  public:
    explicit ProcessRunoffSocont(WaterContainer* container);

    ~ProcessRunoffSocont() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessSettings(SettingsModel* modelSettings);

    /**
     * @copydoc Process::SetHydroUnitProperties()
     */
    void SetHydroUnitProperties(HydroUnit* unit, Brick* brick) override;

    /**
     * @copydoc Process::SetParameters()
     */
    void SetParameters(const ProcessSettings& processSettings) override;

  protected:
    float _slope;           // [m/m]
    const float* _beta;     // []
    double* _areaFraction;  // []
    double _areaUnit;       // [m²]
    double _exponent;

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;

    /**
     * Get the area of the hydro unit.
     *
     * @return The area of the hydro unit [m²]
     */
    [[nodiscard]] double GetArea() const;
};

#endif  // HYDROBRICKS_PROCESS_RUNOFF_SOCONT_H
