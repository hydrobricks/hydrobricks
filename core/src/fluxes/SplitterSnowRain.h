#ifndef HYDROBRICKS_SPLITTER_SNOW_RAIN_H
#define HYDROBRICKS_SPLITTER_SNOW_RAIN_H

#include "Forcing.h"
#include "Includes.h"
#include "Splitter.h"

class SplitterSnowRain : public Splitter {
  public:
    explicit SplitterSnowRain();

    /**
     * @copydoc Splitter::IsOk()
     */
    bool IsOk() override;

    void SetParameters(const SplitterSettings& splitterSettings) override;

    void AttachForcing(Forcing* forcing) override;

    double* GetValuePointer(const string& name) override;

    void Compute() override;

  protected:
    Forcing* m_precipitation;
    Forcing* m_temperature;
    float* m_transitionStart;  // [°C]
    float* m_transitionEnd;    // [°C]

  private:
};

#endif  // HYDROBRICKS_SPLITTER_SNOW_RAIN_H
