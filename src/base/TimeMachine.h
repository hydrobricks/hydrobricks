#ifndef HYDROBRICKS_TIME_MACHINE_H
#define HYDROBRICKS_TIME_MACHINE_H

#include "Includes.h"
#include "ParametersUpdater.h"
#include "SettingsModel.h"

class TimeMachine : public wxObject {
  public:
    TimeMachine();

    ~TimeMachine() override = default;

    void Initialize(const wxDateTime &start, const wxDateTime &end, int timeStep, TimeUnit timeStepUnit);

    void Initialize(const TimerSettings &settings);

    void AttachParametersUpdater(ParametersUpdater* updater) {
        m_parametersUpdater = updater;
    }

    wxDateTime GetDate() {
        return m_date;
    }

    bool IsOver();

    void IncrementTime();

    int GetTimeStepsNb();

  protected:

  private:
    wxDateTime m_date;
    wxDateTime m_start;
    wxDateTime m_end;
    int m_timeStep;
    TimeUnit m_timeStepUnit;
    ParametersUpdater* m_parametersUpdater;
};

#endif  // HYDROBRICKS_TIME_MACHINE_H
