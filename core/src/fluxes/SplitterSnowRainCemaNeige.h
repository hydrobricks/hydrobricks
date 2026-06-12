#ifndef HYDROBRICKS_SPLITTER_SNOW_RAIN_CEMANEIGE_H
#define HYDROBRICKS_SPLITTER_SNOW_RAIN_CEMANEIGE_H

#include <limits>

#include "Forcing.h"
#include "HydroUnit.h"
#include "Includes.h"
#include "Splitter.h"

/**
 * CemaNeige rain/snow splitter (Valéry et al., 2014).
 *
 * Solid fraction formula:
 *   fsolid = max(0, min(1, (Tmax - Tmean) / (Tmax - Tmin)))
 *
 * Temperature interval depends on HydroUnit mean elevation:
 *   elevation >= 1500 m: Tmin = -1 °C, Tmax = 3 °C (hardcoded)
 *   elevation  < 1500 m: Tmin and Tmax are daily observed forcings
 *
 * No calibrated parameters — only forcing inputs.
 */
class SplitterSnowRainCemaNeige : public Splitter {
  public:
    explicit SplitterSnowRainCemaNeige();

    /**
     * @copydoc Splitter::SetParameters()
     */
    void SetParameters(const SplitterSettings& splitterSettings) override;

    /**
     * @copydoc Splitter::AttachForcing()
     */
    void AttachForcing(Forcing* forcing) override;

    /**
     * @copydoc Splitter::GetValuePointer()
     */
    double* GetValuePointer(const string& name) override;

    /**
     * @copydoc Splitter::SetHydroUnitProperties()
     */
    void SetHydroUnitProperties(HydroUnit* unit) override;

    /**
     * @copydoc Splitter::IsValid()
     */
    [[nodiscard]] bool IsValid() const override;

    /**
     * @copydoc Splitter::Compute()
     */
    void Compute() override;

  protected:
    Forcing* _precipitation;
    Forcing* _temperature;
    Forcing* _temperatureMin;
    Forcing* _temperatureMax;
    double _elevation;
    const float* _rainCorrectionFactor;  // [-] multiplies the rain output
    const float* _snowCorrectionFactor;  // [-] multiplies the snow output
};

#endif  // HYDROBRICKS_SPLITTER_SNOW_RAIN_CEMANEIGE_H
