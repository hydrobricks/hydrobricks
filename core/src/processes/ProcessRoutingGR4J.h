#ifndef HYDROBRICKS_PROCESS_ROUTING_GR4J_H
#define HYDROBRICKS_PROCESS_ROUTING_GR4J_H

#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * GR4J routing: unit-hydrograph convolution + non-linear routing store.
 *
 * The uh_input brick accumulates PR = (Pn − Ps) + Perc each timestep.
 * GetRates() computes Q in a read-only fashion (no buffer mutation) so the
 * Heun solver can call it multiple times without corrupting state. The UH
 * buffers and routing store are advanced exactly once per timestep in
 * Finalize(), using the final inflow amounts.
 *
 *  - UH1 (length ceil(X4))   receives 90% of PR
 *  - UH2 (length ceil(2*X4)) receives 10% of PR
 *  - Groundwater exchange: F = X2 × (R/X3)^3.5
 *  - Routing store update (committed in Finalize):
 *      R ← max(0, R + UH1_out + F)
 *      QR = R × (1 − (1 + (R/X3)⁴)^(−1/4))
 *      R ← R − QR
 *  - Direct flow: QD = max(0, UH2_out + F)
 *  - Total: Q = QR + QD → outlet
 */
class ProcessRoutingGR4J : public ProcessOutflow {
  public:
    explicit ProcessRoutingGR4J(WaterContainer* container);

    ~ProcessRoutingGR4J() override = default;

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
     * Advance UH buffers and routing store once per timestep.
     */
    void Finalize() override;

    /**
     * @copydoc Process::GetValuePointer()
     */
    double* GetValuePointer(std::string_view name) override;

  protected:
    const float* _exchangeFactor;   // X2 [mm/d]
    const float* _routingCapacity;  // X3 [mm]
    const float* _uhBaseTime;       // X4 [d]

    // Unit-hydrograph convolution buffers (length changes with X4)
    vecDouble _stuh1;
    vecDouble _stuh2;

    // Pre-computed UH ordinates
    vecDouble _uh1Ord;
    vecDouble _uh2Ord;

    // Routing store level [mm] — updated in Finalize()
    double _r;

    // Logged discharge components exposed via GetValuePointer
    double _qr;
    double _qd;

    // Cached total process-internal water storage for water balance logging
    double _processStorage;

    /**
     * @copydoc Process::GetRates()
     *
     * Read-only: computes Q from current buffer state without modifying it.
     * Buffer advancement happens in Finalize() exactly once per timestep.
     */
    vecDouble GetRates() override;

    /**
     * Recompute UH ordinates and resize buffers based on current X4.
     */
    void _recomputeUH();

    static double _sh1(double t, double x4);
    static double _sh2(double t, double x4);
};

#endif  // HYDROBRICKS_PROCESS_ROUTING_GR4J_H
