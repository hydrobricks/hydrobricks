#ifndef HYDROBRICKS_PROCESS_OUTFLOW_SLOWCOMP_H
#define HYDROBRICKS_PROCESS_OUTFLOW_SLOWCOMP_H

#include "Includes.h"
#include "ProcessOutflow.h"

class ProcessOutflowLinear;

/**
 * PREVAH SLOWCOMP overflow of the fast baseflow store (Schwarze et al., 1999;
 * mxp_runoff.f90 lines 814-832).
 *
 * The fast baseflow store (SLZ1) does not fill like a bucket: it fills
 * asymptotically toward its maximum content with the time constant K1 of its own
 * linear baseflow, so it can only absorb inflow up to (max_content - content)/K1.
 * Any inflow above that overflows to the slow baseflow stores, EVEN when the store
 * is far below its maximum. This diverts high-percolation events (snow melt, storms)
 * from the fast store to the slow ones, which is what sustains the recession:
 *   overflow = max(0, inflow - (max_content - content)/K1)
 *
 * The inflow is the summed incoming change rate of the store's water container (the
 * percolation, plus any wet-surface and firn contributions). K1 is read from the
 * sibling linear baseflow process on the same brick (PREVAH ties the fill and drain
 * time constants). The store itself carries no hard capacity — the asymptotic fill
 * keeps the content below max_content on its own.
 */
class ProcessOutflowSlowcomp : public ProcessOutflow {
  public:
    explicit ProcessOutflowSlowcomp(WaterContainer* container);

    ~ProcessOutflowSlowcomp() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessSettings(SettingsModel* modelSettings);

    /**
     * @copydoc Process::IsValid()
     */
    [[nodiscard]] bool IsValid() const override;

    /**
     * @copydoc Process::SetParameters()
     */
    void SetParameters(const ProcessSettings& processSettings) override;

  protected:
    const float* _maxContent;                  // gws1_max, the SLZ1 maximum content [mm]
    ProcessOutflowLinear* _baseflow{nullptr};  // sibling linear baseflow (cached), for K1

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;

    /**
     * Find the linear baseflow process on the same brick (its response factor is K1).
     *
     * @return pointer to the sibling linear outflow, or nullptr if not found.
     */
    [[nodiscard]] ProcessOutflowLinear* FindSiblingBaseflow() const;
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_SLOWCOMP_H
