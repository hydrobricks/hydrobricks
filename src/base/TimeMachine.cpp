#include "TimeMachine.h"

TimeMachine::TimeMachine()
    : m_timeStep(0),
      m_timeStepUnit(Day),
      m_timeStepInDays(0),
      m_parametersUpdater(nullptr)
{}

void TimeMachine::Initialize(const wxDateTime &start, const wxDateTime &end, int timeStep, TimeUnit timeStepUnit){
    m_date = start;
    m_start = start;
    m_end = end;
    m_timeStep = timeStep;
    m_timeStepUnit = timeStepUnit;
    UpdateTimeStepInDays();
}

void TimeMachine::Initialize(const TimerSettings &settings) {
    if (settings.start.Len() == 10) {
        // Format: YYYY-MM-DD
        if (!m_start.ParseISODate(settings.start)) {
            throw InvalidArgument(_("Incorrect format for the start date."));
        }
    } else if (settings.start.Len() == 19) {
        // Format: YYYY-MM-DDTHH:MM:SS
        if (!m_start.ParseISOCombined(settings.start)) {
            // Try with space separator
            if (!m_start.ParseISOCombined(settings.start, ' ')) {
                throw InvalidArgument(_("Incorrect format for the start date."));
            }
        }
    } else {
        throw InvalidArgument(_("Incorrect format for the start date."));
    }

    if (settings.end.Len() == 10) {
        // Format: YYYY-MM-DD
        if (!m_end.ParseISODate(settings.end)) {
            throw InvalidArgument(_("Incorrect format for the end date."));
        }
    } else if (settings.end.Len() == 19) {
        // Format: YYYY-MM-DDTHH:MM:SS
        if (!m_end.ParseISOCombined(settings.end)) {
            // Try with space separator
            if (!m_end.ParseISOCombined(settings.end, ' ')) {
                throw InvalidArgument(_("Incorrect format for the end date."));
            }
        }
    } else {
        throw InvalidArgument(_("Incorrect format for the end date."));
    }

    m_date = m_start;
    m_timeStep = settings.timeStep;

    if (settings.timeStepUnit.IsSameAs("Day", false)) {
        m_timeStepUnit = Day;
    } else if (settings.timeStepUnit.IsSameAs("Hour", false)) {
        m_timeStepUnit = Hour;
    } else if (settings.timeStepUnit.IsSameAs("Minute", false)) {
        m_timeStepUnit = Minute;
    } else {
        throw InvalidArgument(_("Time step unit unrecognized or not implemented."));
    }

    UpdateTimeStepInDays();
}

bool TimeMachine::IsOver() {
    return m_date.IsLaterThan(m_end);
}

void TimeMachine::IncrementTime() {
    switch (m_timeStepUnit) {
        case Variable:
            wxLogError(_("Variable time step is not yet implemented."));
            wxFAIL;
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

int TimeMachine::GetTimeStepsNb() {
    wxTimeSpan span = m_end.Subtract(m_start);
    switch (m_timeStepUnit) {
        case Variable:
            throw NotImplemented();
        case Week:
            return 1 + span.GetWeeks() / m_timeStep;
        case Day:
            return 1 + span.GetDays() / m_timeStep;
        case Hour:
            return 1 + span.GetHours() / m_timeStep;
        case Minute:
            return 1 + span.GetMinutes() / m_timeStep;
        default:
            throw ShouldNotHappen();
    }
}

void TimeMachine::UpdateTimeStepInDays() {

    switch (m_timeStepUnit) {
        case Variable:
            throw NotImplemented();
        case Week:
            m_timeStepInDays = m_timeStep * 7;
            break;
        case Day:
            m_timeStepInDays = m_timeStep;
            break;
        case Hour:
            m_timeStepInDays = m_timeStep / 24.0;
            break;
        case Minute:
            m_timeStepInDays = m_timeStep / 1440.0;
            break;
        default:
            wxLogError(_("The provided time step unit is not allowed."));
    }
}