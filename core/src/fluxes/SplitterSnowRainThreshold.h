#ifndef HYDROBRICKS_SPLITTER_SNOW_RAIN_THRESHOLD_H
#define HYDROBRICKS_SPLITTER_SNOW_RAIN_THRESHOLD_H

#include "Forcing.h"
#include "Includes.h"
#include "Splitter.h"

/**
 * Rain/snow splitter using a single temperature threshold.
 *
 * All precipitation is snow when T <= threshold, all rain when T > threshold.
 * No intermediate blended zone.
 *
 * Parameters:
 *   threshold [°C]  temperature at and below which all precipitation is solid
 */
class SplitterSnowRainThreshold : public Splitter {
  public:
    explicit SplitterSnowRainThreshold();

    /**
     * @copydoc Splitter::IsValid()
     */
    [[nodiscard]] bool IsValid() const override;

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
     * @copydoc Splitter::Compute()
     */
    void Compute() override;

  protected:
    Forcing* _precipitation;
    Forcing* _temperature;
    const float* _threshold;  // [°C]
};

#endif  // HYDROBRICKS_SPLITTER_SNOW_RAIN_THRESHOLD_H
