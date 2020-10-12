
#ifndef FLHY_TIME_MACHINE_H
#define FLHY_TIME_MACHINE_H

#include "Includes.h"
#include "ParametersUpdater.h"

class TimeMachine : public wxObject {
  public:
    TimeMachine(const wxDateTime &start, const wxDateTime &end, int timeStep, TimeUnit timeStepUnit);

    ~TimeMachine() override = default;

    void AttachParametersUpdater(ParametersUpdater* updater) {
        m_parametersUpdater = updater;
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

#endif  // FLHY_TIME_MACHINE_H
