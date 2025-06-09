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
        return m_date;
    }

    /**
     * Get the start date as a MJD.
     *
     * @return start date
     */
    double GetStart() {
        return m_start;
    }

    /**
     * Get the end date as a MJD.
     *
     * @return end date
     */
    double GetEnd() {
        return m_end;
    }

    /**
     * Get a pointer to the time step in days.
     *
     * @return pointer to the time step in days
     */
    double* GetTimeStepPointer() {
        return &m_timeStepInDays;
    }

    /**
     * Set the parameters updater.
     *
     * @param parametersUpdater pointer to the parameters updater
     */
    void SetParametersUpdater(ParametersUpdater* parametersUpdater) {
        m_parametersUpdater = parametersUpdater;
    }

    /**
     * Set the actions manager.
     *
     * @param actionsManager pointer to the actions manager
     */
    void SetActionsManager(ActionsManager* actionsManager) {
        m_actionsManager = actionsManager;
    }

  private:
    double m_date;
    double m_start;
    double m_end;
    int m_timeStep;
    TimeUnit m_timeStepUnit;
    double m_timeStepInDays;
    ParametersUpdater* m_parametersUpdater;
    ActionsManager* m_actionsManager;

    /**
     * Update the time step in days.
     */
    void UpdateTimeStepInDays();
};

#endif  // HYDROBRICKS_TIME_MACHINE_H
