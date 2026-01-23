#ifndef HYDROBRICKS_SPLITTER_RAIN_H
#define HYDROBRICKS_SPLITTER_RAIN_H

#include "Forcing.h"
#include "Includes.h"
#include "Splitter.h"

class SplitterRain : public Splitter {
  public:
    explicit SplitterRain();

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
    Forcing* _precipitation;  // non-owning reference
};

#endif  // HYDROBRICKS_SPLITTER_RAIN_H
