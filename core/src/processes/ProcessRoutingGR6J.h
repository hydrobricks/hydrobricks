#ifndef HYDROBRICKS_PROCESS_ROUTING_GR6J_H
#define HYDROBRICKS_PROCESS_ROUTING_GR6J_H

#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * GR6J routing (Pushpalatha et al., 2011): two unit hydrographs feeding a
 * non-linear (power) routing store and an exponential store in parallel, plus a
 * direct flow, with a threshold-based groundwater exchange.
 *
 * The uh_input brick accumulates PR = (Pn − Ps) + Perc each timestep.
 * GetRates() computes Q in a read-only fashion (no buffer/store mutation) so the
 * solver can call it multiple times without corrupting state. The UH buffers and
 * both routing stores are advanced exactly once per timestep in Finalize().
 *
 *  - UH1 (length ceil(X4))   receives 90% of PR; its output is split
 *      0.6 → power routing store, 0.4 → exponential store.
 *  - UH2 (length ceil(2*X4)) receives 10% of PR (direct branch).
 *  - Groundwater exchange (added to all three branches):
 *      F = X2 × (R/X3 − X5)
 *  - Power routing store (committed in Finalize):
 *      R ← max(0, R + 0.6·UH1_out + F)
 *      QR = R × (1 − (1 + (R/X3)⁴)^(−1/4)); R ← R − QR
 *  - Exponential store (committed in Finalize; may go negative):
 *      Rexp ← Rexp + 0.4·UH1_out + F
 *      QRExp = gr6j::ExponentialStoreOutflow(Rexp, X6); Rexp ← Rexp − QRExp
 *  - Direct flow: QD = max(0, UH2_out + F)
 *  - Total: Q = max(0, QR + QRExp + QD) → outlet
 */
class ProcessRoutingGR6J : public ProcessOutflow {
  public:
    explicit ProcessRoutingGR6J(WaterContainer* container);

    ~ProcessRoutingGR6J() override = default;

    /**
     * Register the process settings (parameters, forcing, and logging defaults) in the settings model.
     */
    static void RegisterProcessSettings(SettingsModel* modelSettings);

    /**
     * @copydoc Process::SetParameters()
     */
    void SetParameters(const ProcessSettings& processSettings) override;

    /**
     * @copydoc Process::Reset()
     */
    void Reset() override;

    /**
     * Advance UH buffers and both routing stores once per timestep.
     */
    void Finalize() override;

    /**
     * @copydoc Process::GetValuePointer()
     */
    double* GetValuePointer(std::string_view name) override;

    /**
     * @copydoc Process::GetChangeRates()
     *
     * Overridden to bypass the empty-container short-circuit: the bottomless exponential store
     * discharges even when the container is empty.
     */
    [[nodiscard]] vecDouble GetChangeRates() override;

  protected:
    const float* _exchangeFactor;     // X2 [mm/d]
    const float* _routingCapacity;    // X3 [mm]
    const float* _uhBaseTime;         // X4 [d]
    const float* _exchangeThreshold;  // X5 [-]
    const float* _expStoreCoeff;      // X6 [mm]

    // Unit-hydrograph convolution buffers (length changes with X4)
    vecDouble _stuh1;
    vecDouble _stuh2;

    // Pre-computed UH ordinates
    vecDouble _uh1Ord;
    vecDouble _uh2Ord;

    // Store levels [mm] — updated in Finalize()
    double _r;     // power routing store
    double _rexp;  // exponential store (may be negative)

    // Logged discharge components exposed via GetValuePointer
    double _qr;
    double _qrexp;
    double _qd;

    // Cached total process-internal water storage for water balance logging
    double _processStorage;

    /**
     * @copydoc Process::GetRates()
     *
     * Read-only: computes Q from current buffer/store state without modifying it.
     * State advancement happens in Finalize() exactly once per timestep.
     */
    vecDouble GetRates() override;

    /**
     * Recompute UH ordinates and resize buffers based on current X4.
     */
    void _recomputeUH();
};

#endif  // HYDROBRICKS_PROCESS_ROUTING_GR6J_H
