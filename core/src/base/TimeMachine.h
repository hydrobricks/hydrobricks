#ifndef HYDROBRICKS_TIME_MACHINE_H
#define HYDROBRICKS_TIME_MACHINE_H

#include "ActionsManager.h"
#include "Includes.h"
#include "ParametersUpdater.h"
#include "SettingsModel.h"

class TimeMachine {
  public:
    TimeMachine();

    virtual ~TimeMachine() = default;

    /**
     * Initialize the time machine (timer).
     *
     * @param start start date
     * @param end end date
     * @param timeStep time step
     * @param timeStepUnit time step unit
     */
    void Initialize(double start, double end, int timeStep, TimeUnit timeStepUnit);

    /**
     * Initialize the time machine (timer).
     *
     * @param settings settings of the timer
     */
    void Initialize(const TimerSettings& settings);

    /**
     * Reset the timer.
     */
    void Reset();

    /**
     * Check if the timer is over.
     *
     * @return true if the timer is over
     */
    [[nodiscard]] bool IsOver() const;

    /**
     * Increment the timer.
     */
    void IncrementTime();

    /**
     * Get the number of time steps.
     *
     * @return number of time steps
     */
    [[nodiscard]] int GetTimeStepCount() const;

    /**
     * Get the current date as a MJD.
     *
     * @return current date
     */
    [[nodiscard]] double GetDate() const noexcept {
        return _date;
    }

    /**
     * Get the start date as a MJD.
     *
     * @return start date
     */
    [[nodiscard]] double GetStart() const noexcept {
        return _start;
    }

    /**
     * Get the end date as a MJD.
     *
     * @return end date
     */
    [[nodiscard]] double GetEnd() const noexcept {
        return _end;
    }

    /**
     * Get a pointer to the time step in days.
     *
     * @return pointer to the time step in days
     */
    double* GetTimeStepPointer() {
        return &_timeStepInDays;
    }

    /**
     * Set the parameters updater.
     *
     * @param parametersUpdater pointer to the parameters updater
     */
    void SetParametersUpdater(ParametersUpdater* parametersUpdater) {
        _parametersUpdater = parametersUpdater;
    }

    /**
     * Set the actions manager.
     *
     * @param actionsManager pointer to the actions manager
     */
    void SetActionsManager(ActionsManager* actionsManager) {
        _actionsManager = actionsManager;
    }

    /**
     * Get the current day of the year (1-366) from the current date.
     *
     * @return current day of the year.
     */
    [[nodiscard]] int GetCurrentDayOfYear() const;

    /**
     * Check if the time machine is valid.
     * Verifies that start and end dates are properly configured.
     *
     * @return true if the time machine is valid, false otherwise.
     */
    [[nodiscard]] bool IsValid() const;

    /**
     * Validate the time machine.
     * Throws an exception if the time machine is invalid.
     *
     * @throws ModelConfigError if validation fails.
     */
    void Validate() const;

  private:
    double _date;
    double _start;
    double _end;
    int _timeStep;
    TimeUnit _timeStepUnit;
    // The calendar is advanced on an exact integer count of minutes rather than by
    // accumulating the fractional day: 1/24 is not representable in binary, so adding it
    // step after step drifts and can push a timestamp just under an hour boundary, where
    // the truncation that indexes the forcing would pick the previous value. The step
    // index and the step length in minutes are exact, so every date is start + an exact
    // offset. The rates stay per day (_timeStepInDays), which is the unit of the model
    // parameters.
    long long _stepIndex;
    int _timeStepInMinutes;
    double _timeStepInDays;
    ParametersUpdater* _parametersUpdater;  // non-owning reference
    ActionsManager* _actionsManager;        // non-owning reference

    /**
     * Update the time step length (in minutes, and the derived value in days).
     */
    void UpdateTimeStepInDays();

    /**
     * Compute the date of the current step index, from the exact minute offset.
     *
     * @return the date as a MJD.
     */
    [[nodiscard]] double DateAtStepIndex() const;
};

#endif  // HYDROBRICKS_TIME_MACHINE_H
