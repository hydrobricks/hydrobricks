#ifndef HYDROBRICKS_SPLITTER_SNOW_RAIN_CEMANEIGE_H
#define HYDROBRICKS_SPLITTER_SNOW_RAIN_CEMANEIGE_H

#include <limits>

#include "HydroUnit.h"
#include "Includes.h"
#include "SplitterSnowRainLinear.h"

/**
 * CemaNeige rain/snow splitter (Valéry et al., 2014).
 *
 * Same linear temperature interpolation as SplitterSnowRainLinear, but the
 * temperature thresholds depend on HydroUnit mean elevation:
 *
 *   elevation >= 1500 m: Tmin = -1 °C, Tmax = 3 °C (hardcoded)
 *   elevation  < 1500 m: calibrated transition_start / transition_end parameters
 */
class SplitterSnowRainCemaNeige : public SplitterSnowRainLinear {
  public:
    explicit SplitterSnowRainCemaNeige();

    /**
     * @copydoc Splitter::IsValid()
     */
    [[nodiscard]] bool IsValid() const override;

    /**
     * @copydoc Splitter::SetHydroUnitProperties()
     */
    void SetHydroUnitProperties(HydroUnit* unit) override;

    /**
     * @copydoc Splitter::Compute()
     */
    void Compute() override;

  protected:
    double _elevation;  // mean elevation of the HydroUnit [m]
};

#endif  // HYDROBRICKS_SPLITTER_SNOW_RAIN_CEMANEIGE_H
