#ifndef HYDROBRICKS_SPLITTER_SNOW_RAIN_H
#define HYDROBRICKS_SPLITTER_SNOW_RAIN_H

#include "Forcing.h"
#include "Includes.h"
#include "Splitter.h"

class SplitterSnowRain : public Splitter {
  public:
    explicit SplitterSnowRain();

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
    const float* _transitionStart;  // [°C]
    const float* _transitionEnd;    // [°C]
};

#endif  // HYDROBRICKS_SPLITTER_SNOW_RAIN_H
