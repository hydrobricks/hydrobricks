
#ifndef HYDROBRICKS_TIME_MACHINE_H
#define HYDROBRICKS_TIME_MACHINE_H

#include "Includes.h"
#include "ParametersUpdater.h"

class TimeMachine : public wxObject {
  public:
    TimeMachine(const wxDateTime &start, const wxDateTime &end, int timeStep, TimeUnit timeStepUnit);

    ~TimeMachine() override = default;

    void AttachParametersUpdater(ParametersUpdater* updater) {
        m_parametersUpdater = updater;
    }

    wxDateTime GetDate() {
        return m_date;
    }

    bool IsOver();

    void IncrementTime();

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
