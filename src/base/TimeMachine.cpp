#include "TimeMachine.h"

TimeMachine::TimeMachine(const wxDateTime &start, const wxDateTime &end, int timeStep, TimeUnit timeStepUnit)
    : m_date(start),
      m_start(start),
      m_end(end),
      m_timeStep(timeStep),
      m_timeStepUnit(timeStepUnit),
      m_parametersUpdater(nullptr)
{}

bool TimeMachine::IsOver() {
    return !m_date.IsEarlierThan(m_end);
}

void TimeMachine::IncrementTime() {
    switch (m_timeStepUnit) {
        case Variable:
            wxLogError(_("Variable time step is not yet implemented."));
            wxFAIL;
            break;
        case Month:
            m_date.Add(wxDateSpan(0, m_timeStep));
            break;
        case Week:
            m_date.Add(wxDateSpan(0, 0, m_timeStep));
            break;
        case Day:
            m_date.Add(wxDateSpan(0, 0, 0, m_timeStep));
            break;
        case Hour:
            m_date.Add(wxTimeSpan(m_timeStep));
            break;
        case Minute:
            m_date.Add(wxTimeSpan(0, m_timeStep));
            break;
        default:
            wxLogError(_("The provided time step unit is not allowed."));
    }

    if (m_parametersUpdater) {
        m_parametersUpdater->DateUpdate(m_date);
    }
}