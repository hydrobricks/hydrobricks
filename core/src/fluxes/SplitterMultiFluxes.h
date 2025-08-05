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

    /**
     * @copydoc Splitter::SetParameters()
     */
    void SetParameters(const SplitterSettings& splitterSettings) override;

    /**
     * @copydoc Splitter::GetValuePointer()
     */
    double* GetValuePointer(const string& name) override;

    /**
     * @copydoc Splitter::Compute()
     */
    void Compute() override;
};

#endif  // HYDROBRICKS_SPLITTER_MULTI_FLUXES_H
