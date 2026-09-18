#ifndef HYDROBRICKS_PROCESS_ROUTING_DELAY_H
#define HYDROBRICKS_PROCESS_ROUTING_DELAY_H

#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * Pure translation: the inflow of the routing brick leaves it unchanged in shape, a
 * fixed duration later (the translation element of PREVAH's glacier reservoirs).
 *
 * The delay [d] is a duration whatever the time step. On the grid of time steps it is
 * d = delay / timestep steps; the water received during a step is a block one step
 * long, and shifting that block by d steps lands a share (1 - f) of it in step
 * floor(d) and f in the next one, f being the fractional part of d. A delay that is a
 * whole number of steps is therefore an exact shift, and a delay shorter than a step
 * (a one-hour translation on a daily step) spreads a matching share onto the next step
 * instead of being dropped. A zero delay behaves exactly like a pass-through.
 *
 * GetRates() is read-only: it returns what is scheduled for this step plus the
 * same-step share of the water arriving now, read as it enters during the solve (the
 * upstream rates and this step's forcing and instantaneous amounts), so a zero delay
 * makes the brick transparent. Finalize() measures the water that actually arrived,
 * schedules the rest of it, and carries any undelivered amount over so no water is
 * stranded in the brick.
 */
class ProcessRoutingDelay : public ProcessOutflow {
  public:
    explicit ProcessRoutingDelay(WaterContainer* container);

    ~ProcessRoutingDelay() override = default;

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
     * Advance the delivery schedule once per time step, using the committed amounts.
     */
    void Finalize() override;

    /**
     * @copydoc Process::GetChangeRates()
     *
     * Bypasses the empty-container shortcut of the base class, as routing:hbv does: the
     * scheduled delivery usually drains the whole content within the step.
     */
    [[nodiscard]] const vecDouble& GetChangeRates() override {
        return GetRates();
    }

    /**
     * @copydoc Process::GetValuePointer()
     */
    double* GetValuePointer(std::string_view name) override;

  protected:
    const float* _delay;  // translation time [d]

    // Delivery schedule (slot j: water due j time steps from now) and its ordinates.
    vecDouble _schedule;
    vecDouble _ordinates;
    double _lastDelay;
    double _lastTimeStep;

    // End-of-step content of the previous step (to measure the committed inflow).
    double _previousContent;

    // In-transit water (scheduled, not yet delivered) for water balance logging.
    double _processStorage;

    /**
     * @copydoc Process::GetRates()
     *
     * Read-only: the delivery due this step, from the current schedule.
     */
    const vecDouble& GetRates() override;

    /**
     * Recompute the ordinates and resize the schedule for the current delay and step.
     */
    void _recomputeOrdinates();
};

#endif  // HYDROBRICKS_PROCESS_ROUTING_DELAY_H
