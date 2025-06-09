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
    Forcing* m_precipitation;
    Forcing* m_temperature;
    float* m_transitionStart;  // [°C]
    float* m_transitionEnd;    // [°C]
};

#endif  // HYDROBRICKS_SPLITTER_SNOW_RAIN_H
