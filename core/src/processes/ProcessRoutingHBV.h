#ifndef HYDROBRICKS_PROCESS_ROUTING_HBV_H
#define HYDROBRICKS_PROCESS_ROUTING_HBV_H

#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * HBV transformation function: triangular unit-hydrograph routing (MAXBAS).
 *
 * The routing brick accumulates the runoff components (e.g. upper and lower
 * zone outflows) each timestep. The total is convolved with a triangular
 * weighting function of base maxbas [d]:
 *   - cumulative weights C(t) = 2(t/m)^2 for t <= m/2
 *   -                    C(t) = 1 - 2(1 - t/m)^2 for m/2 < t <= m
 *   - ordinates w_j = C(j) - C(j-1), j = 1..ceil(m)
 *
 * With maxbas <= 1 the inflow is passed through within the timestep.
 *
 * The process maintains a delivery schedule (_stuh): each slot holds the water
 * due to leave that many timesteps from now. GetRates() is read-only (the
 * solver can call it multiple times without corrupting state) and returns the
 * amount due this timestep plus the same-step share of the current inflow. The
 * schedule is advanced exactly once per timestep in Finalize(), based on the
 * committed amounts: the actually received inflow is distributed over the
 * ordinates and any difference between the scheduled and the actual delivery
 * (solver stage averaging, rate constraints) is carried over to the next
 * timestep, so no water gets stranded in the routing container.
 */
class ProcessRoutingHBV : public ProcessOutflow {
  public:
    explicit ProcessRoutingHBV(WaterContainer* container);

    ~ProcessRoutingHBV() override = default;

    /**
     * Register the process settings (parameters and logging defaults) in the settings model.
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

    /**
     * @copydoc Process::Reset()
     */
    void Reset() override;

    /**
     * Advance the delivery schedule once per timestep, using the committed amounts.
     */
    void Finalize() override;

    /**
     * @copydoc Process::GetChangeRates()
     *
     * Bypasses the empty-container shortcut of the base class: the scheduled
     * delivery typically drains the whole container content within the timestep,
     * which would zero the rates at later solver stages and halve the delivery.
     * The container constraints still prevent any negative content.
     */
    [[nodiscard]] vecDouble GetChangeRates() override {
        return GetRates();
    }

    /**
     * @copydoc Process::GetValuePointer()
     */
    double* GetValuePointer(std::string_view name) override;

  protected:
    const float* _maxbas;  // base of the triangular weighting function [d]

    // Delivery schedule (slot j: water due j timesteps from now) and pre-computed ordinates
    vecDouble _stuh;
    vecDouble _uhOrd;
    double _lastMaxbas;

    // End-of-step container content of the previous timestep (to measure the committed inflow)
    double _previousContent;

    // In-transit water (scheduled, not yet delivered) for water balance logging
    double _processStorage;

    /**
     * @copydoc Process::GetRates()
     *
     * Read-only: computes the scheduled delivery from the current schedule state
     * without modifying it. Schedule advancement happens in Finalize().
     */
    vecDouble GetRates() override;

    /**
     * Recompute the UH ordinates and resize the buffer based on the current maxbas.
     */
    void _recomputeUH();

    /**
     * Cumulative triangular weighting function.
     */
    static double _cumulativeWeight(double t, double maxbas);
};

#endif  // HYDROBRICKS_PROCESS_ROUTING_HBV_H
