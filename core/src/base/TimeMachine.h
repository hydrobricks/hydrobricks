#ifndef HYDROBRICKS_TIME_MACHINE_H
#define HYDROBRICKS_TIME_MACHINE_H

#include "BehavioursManager.h"
#include "Includes.h"
#include "ParametersUpdater.h"
#include "SettingsModel.h"

class TimeMachine : public wxObject {
  public:
    TimeMachine();

    ~TimeMachine() override = default;

    void Initialize(double start, double end, int timeStep, TimeUnit timeStepUnit);

    void Initialize(const TimerSettings& settings);

    void Reset();

    bool IsOver();

    void IncrementTime();

    int GetTimeStepsNb();

    void UpdateTimeStepInDays();

    double GetDate() {
        return m_date;
    }

    double GetStart() {
        return m_start;
    }

    double GetEnd() {
        return m_end;
    }

    double* GetTimeStepPointer() {
        return &m_timeStepInDays;
    }

    void SetParametersUpdater(ParametersUpdater* parametersUpdater) {
        m_parametersUpdater = parametersUpdater;
    }

    void SetBehavioursManager(BehavioursManager* behavioursManager) {
        m_behavioursManager = behavioursManager;
    }

  protected:
  private:
    double m_date;
    double m_start;
    double m_end;
    int m_timeStep;
    TimeUnit m_timeStepUnit;
    double m_timeStepInDays;
    ParametersUpdater* m_parametersUpdater;
    BehavioursManager* m_behavioursManager;
};

#endif  // HYDROBRICKS_TIME_MACHINE_H
