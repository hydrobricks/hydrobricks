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

    void AssignParameters(const SplitterSettings& splitterSettings) override;

    void AttachForcing(Forcing* forcing) override;

    double* GetValuePointer(const std::string& name) override;

    void Compute() override;

  protected:
    Forcing* m_precipitation;

  private:
};

#endif  // HYDROBRICKS_SPLITTER_RAIN_H
