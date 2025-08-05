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
    bool IsOver();

    /**
     * Increment the timer.
     */
    void IncrementTime();

    /**
     * Get the number of time steps.
     *
     * @return number of time steps
     */
    int GetTimeStepsNb();

    /**
     * Get the current date as a MJD.
     *
     * @return current date
     */
    double GetDate() {
        return _date;
    }

    /**
     * Get the start date as a MJD.
     *
     * @return start date
     */
    double GetStart() {
        return _start;
    }

    /**
     * Get the end date as a MJD.
     *
     * @return end date
     */
    double GetEnd() {
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

  private:
    double _date;
    double _start;
    double _end;
    int _timeStep;
    TimeUnit _timeStepUnit;
    double _timeStepInDays;
    ParametersUpdater* _parametersUpdater;
    ActionsManager* _actionsManager;

    /**
     * Update the time step in days.
     */
    void UpdateTimeStepInDays();
};

#endif  // HYDROBRICKS_TIME_MACHINE_H
