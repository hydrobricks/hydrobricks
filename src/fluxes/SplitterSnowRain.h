#ifndef HYDROBRICKS_SPLITTER_SNOW_RAIN_H
#define HYDROBRICKS_SPLITTER_SNOW_RAIN_H

#include "Includes.h"
#include "Splitter.h"
#include "Forcing.h"

class SplitterSnowRain : public Splitter {
  public:
    explicit SplitterSnowRain(HydroUnit *hydroUnit);

    /**
     * @copydoc Splitter::IsOk()
     */
    bool IsOk() override;

    void AssignParameters(const SplitterSettings &splitterSettings) override;

    void AttachForcing(Forcing* forcing) override;

    double* GetValuePointer(const wxString& name) override;

    void Compute() override;

  protected:
    Forcing* m_precipitation;
    Forcing* m_temperature;
    float* m_transitionStart;  // [°C]
    float* m_transitionEnd;  // [°C]

  private:
};

#endif  // HYDROBRICKS_SPLITTER_SNOW_RAIN_H
