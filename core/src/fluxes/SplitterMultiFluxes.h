#ifndef HYDROBRICKS_SPLITTER_MULTI_FLUXES_H
#define HYDROBRICKS_SPLITTER_MULTI_FLUXES_H

#include "Includes.h"
#include "Splitter.h"

class Flux;

class SplitterMultiFluxes : public Splitter {
  public:
    explicit SplitterMultiFluxes();

    /**
     * @copydoc Splitter::IsOk()
     */
    bool IsOk() override;

    void AssignParameters(const SplitterSettings& splitterSettings) override;

    double* GetValuePointer(const std::string& name) override;

    void Compute() override;

  protected:
  private:
};

#endif  // HYDROBRICKS_SPLITTER_MULTI_FLUXES_H
