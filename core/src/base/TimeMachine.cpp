#include "TimeMachine.h"

TimeMachine::TimeMachine()
    : m_timeStep(0),
      m_timeStepUnit(Day),
      m_timeStepInDays(0),
      m_parametersUpdater(nullptr)
{}

void TimeMachine::Initialize(double start, double end, int timeStep, TimeUnit timeStepUnit){
    m_date = start;
    m_start = start;
    m_end = end;
    m_timeStep = timeStep;
    m_timeStepUnit = timeStepUnit;
    UpdateTimeStepInDays();
}

void TimeMachine::Initialize(const TimerSettings &settings) {

    m_start = ParseDate(settings.start, guess);
    m_end = ParseDate(settings.end, guess);
    m_date = m_start;
    m_timeStep = settings.timeStep;

    if (settings.timeStepUnit == "Day") {
        m_timeStepUnit = Day;
    } else if (settings.timeStepUnit == "Hour") {
        m_timeStepUnit = Hour;
    } else if (settings.timeStepUnit == "Minute") {
        m_timeStepUnit = Minute;
    } else {
        throw InvalidArgument(_("Time step unit unrecognized or not implemented."));
    }

    UpdateTimeStepInDays();
}

bool TimeMachine::IsOver() {
    return m_date > m_end;
}

void TimeMachine::IncrementTime() {
    wxASSERT(m_timeStepInDays > 0);
    m_date += m_timeStepInDays;

    if (m_parametersUpdater) {
        m_parametersUpdater->DateUpdate(m_date);
    }
}

int TimeMachine::GetTimeStepsNb() {
    wxASSERT(m_timeStepInDays > 0);
    return int(1 + (m_end - m_start) / m_timeStepInDays);
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