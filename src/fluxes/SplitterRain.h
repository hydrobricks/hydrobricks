#ifndef HYDROBRICKS_SPLITTER_RAIN_H
#define HYDROBRICKS_SPLITTER_RAIN_H

#include "Includes.h"
#include "Splitter.h"
#include "Forcing.h"

class SplitterRain : public Splitter {
  public:
    explicit SplitterRain();

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

  private:
};

#endif  // HYDROBRICKS_SPLITTER_RAIN_H
