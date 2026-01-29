#ifndef HYDROBRICKS_TIME_MACHINE_H
#define HYDROBRICKS_TIME_MACHINE_H

#include "ActionsManager.h"
#include "Includes.h"
#include "ParametersUpdater.h"
#include "SettingsModel.h"

class TimeMachine : public wxObject {
  public:
    TimeMachine();

    ~TimeMachine() override = default;

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
    int GetTimeStepCount() const;

    /**
     * Get the current date as a MJD.
     *
     * @return current date
     */
    double GetDate() const {
        return _date;
    }

    /**
     * Get the start date as a MJD.
     *
     * @return start date
     */
    double GetStart() const {
        return _start;
    }

    /**
     * Get the end date as a MJD.
     *
     * @return end date
     */
    double GetEnd() const {
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
     * Get the current date (MJD) from the global timer state.
     *
     * @return current date as MJD.
     */
    static double GetCurrentDate() {
        return _currentDateStatic;
    }

    /**
     * Get the current day of the year (1-366) from the static current date.
     *
     * @return current day of the year.
     */
    static int GetCurrentDayOfYear();

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
    double _timeStepInDays;
    ParametersUpdater* _parametersUpdater;  // non-owning reference
    ActionsManager* _actionsManager;        // non-owning reference
    static double _currentDateStatic;       // Holds the globally accessible current date (MJD)

    /**
     * Update the time step in days.
     */
    void UpdateTimeStepInDays();
};

#endif  // HYDROBRICKS_TIME_MACHINE_H
