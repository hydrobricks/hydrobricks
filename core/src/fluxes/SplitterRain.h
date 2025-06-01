#ifndef HYDROBRICKS_SPLITTER_RAIN_H
#define HYDROBRICKS_SPLITTER_RAIN_H

#include "Forcing.h"
#include "Includes.h"
#include "Splitter.h"

class SplitterRain : public Splitter {
  public:
    explicit SplitterRain();

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
};

#endif  // HYDROBRICKS_SPLITTER_RAIN_H
