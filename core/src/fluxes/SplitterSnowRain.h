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
    Forcing* _precipitation;
    Forcing* _temperature;
    float* _transitionStart;  // [°C]
    float* _transitionEnd;    // [°C]
};

#endif  // HYDROBRICKS_SPLITTER_SNOW_RAIN_H
